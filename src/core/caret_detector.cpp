#include "caret_detector.h"
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <vector>
#include <atspi/atspi.h>
#include <glib.h>

static std::atomic<bool> s_is_terminal{false};
static std::thread s_listener_thread;
static GMainLoop* s_glib_loop = nullptr;
static std::atomic<bool> s_running{false};

static bool isTerminalKeyword(const std::string& name) {
    if (name.empty()) return false;
    std::string lower = name;
    for (char& c : lower) c = std::tolower(c);
    static const std::vector<std::string> kw = {
        "terminal", "ptyxis", "konsole", "kitty", "alacritty",
        "wezterm", "terminator", "tilix", "foot", "urxvt", "xterm",
        "tilda", "guake", "yakuake", "console"
    };
    for (const auto& k : kw) {
        if (lower.find(k) != std::string::npos) return true;
    }
    return false;
}

static void on_window_activated(AtspiEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->source) return;

    AtspiAccessible* app = atspi_accessible_get_application(event->source, nullptr);
    const char* app_name = app ? atspi_accessible_get_name(app, nullptr) : nullptr;
    std::string app_str = app_name ? app_name : "";
    if (app) {
        g_object_unref(app);
    }

    if (app_str == "cliptraylt" || app_str.find("cliptray") != std::string::npos || app_str == "gnome-shell") {
        return;
    }

    AtspiRole role = atspi_accessible_get_role(event->source, nullptr);
    bool is_term = (role == ATSPI_ROLE_TERMINAL || isTerminalKeyword(app_str));
    s_is_terminal.store(is_term, std::memory_order_relaxed);
    std::cout << "[CaretDetector] Active window changed: '" << app_str
              << "' -> terminal=" << (is_term ? "YES" : "NO") << "\n" << std::flush;
}

static void ensureAtspiBusAddress() {
    if (!std::getenv("AT_SPI_BUS_ADDRESS")) {
        std::string uid = "1000";
        const char* sudo_user = std::getenv("SUDO_USER");
        if (sudo_user) {
            struct passwd* pw = getpwnam(sudo_user);
            if (pw) uid = std::to_string(pw->pw_uid);
        }
        std::string addr = "unix:path=/run/user/" + uid + "/at-spi/bus";
        setenv("AT_SPI_BUS_ADDRESS", addr.c_str(), 1);
    }
}

void CaretDetector::runEventLoop() {
    ensureAtspiBusAddress();
    if (atspi_init() != 0) {
        std::cerr << "[CaretDetector] Failed to initialize AT-SPI bus.\n";
        return;
    }

    // Do a quick one-time initial desktop check purely in the background thread at startup
    AtspiAccessible* desktop = atspi_get_desktop(0);
    if (desktop) {
        int count = atspi_accessible_get_child_count(desktop, nullptr);
        for (int i = 0; i < count; ++i) {
            AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, nullptr);
            if (!app) continue;
            const char* aname = atspi_accessible_get_name(app, nullptr);
            std::string app_str = aname ? aname : "";
            if (app_str != "cliptraylt" && app_str != "gnome-shell") {
                int win_count = atspi_accessible_get_child_count(app, nullptr);
                for (int j = 0; j < win_count; ++j) {
                    AtspiAccessible* win = atspi_accessible_get_child_at_index(app, j, nullptr);
                    if (!win) continue;
                    AtspiStateSet* states = atspi_accessible_get_state_set(win);
                    if (states) {
                        if (atspi_state_set_contains(states, ATSPI_STATE_ACTIVE) ||
                            atspi_state_set_contains(states, ATSPI_STATE_FOCUSED)) {
                            AtspiRole role = atspi_accessible_get_role(win, nullptr);
                            if (role == ATSPI_ROLE_TERMINAL || isTerminalKeyword(app_str)) {
                                s_is_terminal.store(true, std::memory_order_relaxed);
                            }
                        }
                        g_object_unref(states);
                    }
                    g_object_unref(win);
                }
            }
            g_object_unref(app);
        }
        g_object_unref(desktop);
    }

    AtspiEventListener* listener = atspi_event_listener_new(on_window_activated, nullptr, nullptr);
    atspi_event_listener_register(listener, "window:activate", nullptr);

    std::cout << "[CaretDetector] Zero-overhead AT-SPI window tracking active.\n" << std::flush;

    s_glib_loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(s_glib_loop);

    g_object_unref(listener);
}

void CaretDetector::initialize() {
    if (s_running) return;
    s_running = true;
    s_listener_thread = std::thread(&CaretDetector::runEventLoop);
}

void CaretDetector::shutdown() {
    if (!s_running) return;
    s_running = false;
    if (s_glib_loop) {
        g_main_loop_quit(s_glib_loop);
    }
    if (s_listener_thread.joinable()) {
        s_listener_thread.join();
    }
}

bool CaretDetector::isTerminalActive() {
    // Pure atomic read: 0 nanoseconds, zero D-Bus calls, zero chance of locking the main thread!
    return s_is_terminal.load(std::memory_order_relaxed);
}

