#include "evdev_listener.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <libevdev/libevdev.h>

namespace fs = std::filesystem;

EvdevListener::EvdevListener(QObject* parent) : QObject(parent) {}

EvdevListener::~EvdevListener() {
    stop();
}

bool EvdevListener::start() {
    if (running_) return true;
    discoverDevices();
    if (input_fds_.empty()) {
        std::cerr << "[EvdevListener] No input devices found. Running as root? (e.g. sudo ./run.sh --root)\n";
        return false;
    }

    running_ = true;
    worker_thread_ = std::thread(&EvdevListener::runLoop, this);
    std::cout << "[EvdevListener] Ready! Monitoring " << input_fds_.size()
              << " hardware input devices (keyboards & mice).\n";
    return true;
}

void EvdevListener::stop() {
    if (!running_) return;
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    closeDevices();
}

void EvdevListener::discoverDevices() {
    closeDevices();

    for (const auto& entry : fs::directory_iterator("/dev/input")) {
        std::string path = entry.path().string();
        if (path.find("/event") == std::string::npos) continue;

        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        struct libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            close(fd);
            continue;
        }

        const char* name = libevdev_get_name(dev);
        std::string sname = name ? name : "";

        // Skip virtual clipboard keyboard
        if (sname.find("SimpleClipboard") != std::string::npos ||
            sname.find("virtual-clipboard") != std::string::npos) {
            libevdev_free(dev);
            close(fd);
            continue;
        }

        bool is_keyboard = libevdev_has_event_type(dev, EV_KEY) &&
                           (libevdev_has_event_code(dev, EV_KEY, KEY_LEFTMETA) ||
                            libevdev_has_event_code(dev, EV_KEY, KEY_RIGHTMETA)) &&
                           libevdev_has_event_code(dev, EV_KEY, KEY_V);

        bool is_mouse = libevdev_has_event_type(dev, EV_KEY) &&
                        (libevdev_has_event_code(dev, EV_KEY, BTN_LEFT) ||
                         libevdev_has_event_code(dev, EV_KEY, BTN_RIGHT));

        if (is_keyboard || is_mouse) {
            std::cout << "[EvdevListener] Monitoring "
                      << (is_keyboard ? "Keyboard" : "Mouse")
                      << ": " << sname << " (" << path << ")\n";
            input_fds_.push_back(fd);
        } else {
            close(fd);
        }
        libevdev_free(dev);
    }
}

void EvdevListener::closeDevices() {
    for (int fd : input_fds_) {
        close(fd);
    }
    input_fds_.clear();
}

void EvdevListener::runLoop() {
    if (input_fds_.empty()) return;

    std::vector<struct libevdev*> evdevs;
    std::vector<struct pollfd> pfds;

    for (int fd : input_fds_) {
        struct libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) == 0) {
            evdevs.push_back(dev);
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            pfds.push_back(pfd);
        }
    }

    auto last_hotkey_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (running_) {
        int ret = poll(pfds.data(), pfds.size(), 10); // 50ms poll
        if (ret <= 0) continue;

        for (size_t i = 0; i < pfds.size(); ++i) {
            if (pfds[i].revents & POLLIN) {
                struct input_event ev;
                while (libevdev_next_event(evdevs[i], LIBEVDEV_READ_FLAG_NORMAL, &ev) == LIBEVDEV_READ_STATUS_SUCCESS) {
                    if (ev.type != EV_KEY) continue;

                    // 1. Keyboard Super + V tracking
                    if (ev.code == KEY_V && ev.value == 1) { // V pressed down
                        // Query real-time kernel state of modifier keys across all connected keyboards
                        bool super_down = false;
                        bool ctrl_down = false;
                        bool alt_down = false;

                        for (auto* d : evdevs) {
                            if (libevdev_get_event_value(d, EV_KEY, KEY_LEFTMETA) ||
                                libevdev_get_event_value(d, EV_KEY, KEY_RIGHTMETA)) {
                                super_down = true;
                            }
                            if (libevdev_get_event_value(d, EV_KEY, KEY_LEFTCTRL) ||
                                libevdev_get_event_value(d, EV_KEY, KEY_RIGHTCTRL)) {
                                ctrl_down = true;
                            }
                            if (libevdev_get_event_value(d, EV_KEY, KEY_LEFTALT) ||
                                libevdev_get_event_value(d, EV_KEY, KEY_RIGHTALT)) {
                                alt_down = true;
                            }
                        }

                        // STRICT CONDITION: Only trigger when SUPER is physically held down,
                        // and neither CTRL nor ALT is held down!
                        if (super_down && !ctrl_down && !alt_down) {
                            auto now = std::chrono::steady_clock::now();
                            auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_hotkey_time).count();
                            if (diff > 180) { // debounce
                                last_hotkey_time = now;
                                std::cout << "\n[Evdev] Verified hardware Super+V pressed. Toggling flyout.\n" << std::flush;
                                emit hotkeyTriggered();
                            }
                        }
                    }
                    // 2. Escape Key tracking
                    else if (ev.code == KEY_ESC && ev.value == 1) {
                        emit escapePressed();
                    }
                    // 3. Global Mouse Click tracking (Clicks outside window)
                    else if ((ev.code == BTN_LEFT || ev.code == BTN_RIGHT) && ev.value == 1) {
                        emit globalMousePressed();
                    }
                }
            }
        }
    }

    for (auto* dev : evdevs) {
        libevdev_free(dev);
    }
}
