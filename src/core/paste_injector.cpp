#include "paste_injector.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <linux/uinput.h>

PasteInjector::PasteInjector() {
    init();
}

PasteInjector::~PasteInjector() {
    if (uinput_fd_ >= 0) {
        ioctl(uinput_fd_, UI_DEV_DESTROY);
        close(uinput_fd_);
        uinput_fd_ = -1;
    }
}

bool PasteInjector::init() {
    if (uinput_fd_ >= 0) return true;

    uinput_fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinput_fd_ < 0) {
        std::cerr << "[PasteInjector] Failed to open /dev/uinput. Are you running as root or part of input group?\n";
        return false;
    }

    ioctl(uinput_fd_, UI_SET_EVBIT, EV_KEY);
    ioctl(uinput_fd_, UI_SET_EVBIT, EV_SYN);

    ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_LEFTCTRL);
    ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_RIGHTCTRL);
    ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_INSERT);
    ioctl(uinput_fd_, UI_SET_KEYBIT, KEY_V);

    struct uinput_setup usetup;
    std::memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    std::strncpy(usetup.name, "ClipTrayLT Virtual Keyboard", UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(uinput_fd_, UI_DEV_SETUP, &usetup) < 0) {
        std::cerr << "[PasteInjector] UI_DEV_SETUP failed\n";
        close(uinput_fd_);
        uinput_fd_ = -1;
        return false;
    }

    if (ioctl(uinput_fd_, UI_DEV_CREATE) < 0) {
        std::cerr << "[PasteInjector] UI_DEV_CREATE failed\n";
        close(uinput_fd_);
        uinput_fd_ = -1;
        return false;
    }

    std::cout << "[PasteInjector] Initialized virtual hardware keyboard successfully.\n";
    return true;
}

void PasteInjector::emitKey(int type, int code, int val) {
    if (uinput_fd_ < 0) return;
    struct input_event ie;
    std::memset(&ie, 0, sizeof(ie));
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ssize_t ret = write(uinput_fd_, &ie, sizeof(ie));
    (void)ret;
}

bool PasteInjector::paste(int delay_ms) {
    std::cout << "[PasteInjector] PASTE INJECTED! delay=" << delay_ms << std::endl << std::flush;
    if (uinput_fd_ < 0) {
        if (!init()) return false;
    }

    if (delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    emitKey(EV_KEY, KEY_LEFTCTRL, 1);
    emitKey(EV_SYN, SYN_REPORT, 0);

    emitKey(EV_KEY, KEY_V, 1);
    emitKey(EV_SYN, SYN_REPORT, 0);

    emitKey(EV_KEY, KEY_V, 0);
    emitKey(EV_SYN, SYN_REPORT, 0);

    emitKey(EV_KEY, KEY_LEFTCTRL, 0);
    emitKey(EV_SYN, SYN_REPORT, 0);

    return true;
}
