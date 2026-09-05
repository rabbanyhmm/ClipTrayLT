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

            const char* app_name = atspi_accessible_get_name(atspi_accessible_get_application(event->source, nullptr), nullptr);
            std::cout << "[CaretDetector] Caret position updated from '" << (app_name ? app_name : "App")
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

    std::cout << "[CaretDetector] Real-time background AT-SPI caret tracking active.\n" << std::flush;

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
