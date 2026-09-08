#include "caret_detector.h"
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <chrono>
#include <atspi/atspi.h>
#include <glib.h>

static std::atomic<int> s_caret_x{0};
static std::atomic<int> s_caret_y{0};
static std::atomic<int> s_caret_w{0};
static std::atomic<int> s_caret_h{0};
static std::atomic<int64_t> s_last_caret_time{0};
static std::atomic<bool> s_has_active_text{false};
static std::atomic<bool> s_is_terminal{false};

static bool isTerminalKeyword(const std::string& name) {
    if (name.empty()) return false;
    std::string lower = name;
    for (char& c : lower) c = std::tolower(c);
    const std::vector<std::string> kw = {
        "terminal", "ptyxis", "konsole", "kitty", "alacritty",
        "wezterm", "terminator", "tilix", "foot", "urxvt", "xterm",
        "tilda", "guake", "yakuake", "console"
    };
    for (const auto& k : kw) {
        if (lower.find(k) != std::string::npos) return true;
    }
    return false;
}

static std::thread s_listener_thread;
static GMainLoop* s_glib_loop = nullptr;
static std::atomic<bool> s_running{false};

static int64_t getNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void on_caret_moved(AtspiEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->source) return;

    AtspiAccessible* app = atspi_accessible_get_application(event->source, nullptr);
    const char* app_name = app ? atspi_accessible_get_name(app, nullptr) : nullptr;
    std::string app_str = app_name ? app_name : "";
    if (app) {
        g_object_unref(app);
    }

    // Ignore events originating from our own flyout window or system shell overlay
    if (app_str == "cliptraylt" || app_str.find("cliptray") != std::string::npos || app_str == "gnome-shell") {
        return;
    }

    AtspiRole role = atspi_accessible_get_role(event->source, nullptr);
    if (role == ATSPI_ROLE_TERMINAL || isTerminalKeyword(app_str)) {
        s_is_terminal = true;
    } else {
        s_is_terminal = false;
    }

    AtspiText* text = atspi_accessible_get_text_iface(event->source);
    if (!text) {
        // Focused element does not support text input
        s_has_active_text = false;
        return;
    }

    gint offset = atspi_text_get_caret_offset(text, nullptr);
    AtspiRect* r = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, nullptr);
    if (r) {
        if (r->x > 0 || r->y > 0) {
            s_caret_x = r->x;
            s_caret_y = r->y;
            s_caret_w = r->width;
            s_caret_h = r->height > 0 ? r->height : 22;
            s_last_caret_time = getNowMs();
            s_has_active_text = true;

            std::cout << "[CaretDetector] Caret position updated from '" << (!app_str.empty() ? app_str : "App")
                      << "': (" << r->x << ", " << r->y << ")\n" << std::flush;
        }
        g_free(r);
    }
    g_object_unref(text);
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

    AtspiEventListener* listener = atspi_event_listener_new(on_caret_moved, nullptr, nullptr);
    atspi_event_listener_register(listener, "object:text-caret-moved", nullptr);
    atspi_event_listener_register(listener, "object:state-changed:focused", nullptr);
    atspi_event_listener_register(listener, "window:activate", nullptr);

    std::cout << "[CaretDetector] Real-time background AT-SPI caret and terminal tracking active.\n" << std::flush;

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

std::optional<CaretInfo> CaretDetector::getActiveTextCaret() {
    if (!s_has_active_text) {
        std::cout << "[CaretDetector] No active text input detected. Anchoring flyout to bottom-right corner.\n";
        return std::nullopt;
    }

    // Check if the last caret movement was within the last 60 seconds
    int64_t diff = getNowMs() - s_last_caret_time.load();
    if (diff > 60000) {
        std::cout << "[CaretDetector] Last caret event expired (>60s). Anchoring to bottom-right corner.\n";
        return std::nullopt;
    }

    CaretInfo info;
    info.x = s_caret_x.load();
    info.y = s_caret_y.load();
    info.width = s_caret_w.load();
    info.height = s_caret_h.load();

    std::cout << "[CaretDetector] Returning active typing caret: X=" << info.x
              << ", Y=" << info.y << ", H=" << info.height << "\n" << std::flush;
    return info;
}

bool CaretDetector::isTerminalActive() {
    ensureAtspiBusAddress();
    AtspiAccessible* desktop = atspi_get_desktop(0);
    if (desktop) {
        int count = atspi_accessible_get_child_count(desktop, nullptr);
        for (int i = 0; i < count; ++i) {
            AtspiAccessible* app = atspi_accessible_get_child_at_index(desktop, i, nullptr);
            if (!app) continue;

            const char* aname = atspi_accessible_get_name(app, nullptr);
            std::string app_str = aname ? aname : "";
            if (app_str == "cliptraylt" || app_str.find("cliptray") != std::string::npos || app_str == "gnome-shell") {
                g_object_unref(app);
                continue;
            }

            int win_count = atspi_accessible_get_child_count(app, nullptr);
            for (int j = 0; j < win_count; ++j) {
                AtspiAccessible* win = atspi_accessible_get_child_at_index(app, j, nullptr);
                if (!win) continue;

                AtspiStateSet* states = atspi_accessible_get_state_set(win);
                if (states) {
                    bool active = atspi_state_set_contains(states, ATSPI_STATE_ACTIVE) ||
                                  atspi_state_set_contains(states, ATSPI_STATE_FOCUSED);
                    g_object_unref(states);
                    if (active) {
                        AtspiRole role = atspi_accessible_get_role(win, nullptr);
                        bool is_term = (role == ATSPI_ROLE_TERMINAL || isTerminalKeyword(app_str));
                        g_object_unref(win);
                        g_object_unref(app);
                        g_object_unref(desktop);
                        s_is_terminal = is_term;
                        std::cout << "[CaretDetector] Active window detected: '" << app_str
                                  << "' -> is_terminal=" << (is_term ? "YES" : "NO") << "\n" << std::flush;
                        return is_term;
                    }
                }
                g_object_unref(win);
            }
            g_object_unref(app);
        }
        g_object_unref(desktop);
    }

    return s_is_terminal.load();
}

