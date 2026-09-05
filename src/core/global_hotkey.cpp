#include "global_hotkey.h"
#include <iostream>
#include <QtGui/QGuiApplication>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>

#undef Bool
#undef Cursor
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef KeyPress
#undef KeyRelease
#undef None
#undef Status
#undef Unsorted

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {
}

GlobalHotkey::~GlobalHotkey() {
    unregisterHotkey();
}

bool GlobalHotkey::registerHotkey() {
    if (registered_) return true;

    auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11) {
        std::cerr << "[GlobalHotkey] QX11Application native interface not available.\n";
        return false;
    }

    dpy_ = x11->display();
    if (!dpy_) {
        std::cerr << "[GlobalHotkey] X11 Display is null.\n";
        return false;
    }

    keycode_v_ = XKeysymToKeycode(dpy_, XK_v);
    if (keycode_v_ == 0) {
        std::cerr << "[GlobalHotkey] Failed to resolve keycode for 'v'.\n";
        return false;
    }

    Window root = DefaultRootWindow(dpy_);

    // Set error handler temporarily to ignore any X11 grab errors
    auto old_handler = XSetErrorHandler([](Display*, XErrorEvent*) -> int {
        return 0;
    });

    // Grab Super + V across modifier combinations (NumLock, CapsLock, ScrollLock)
    unsigned int masks[] = {
        Mod4Mask,
        Mod4Mask | Mod2Mask,             // NumLock
        Mod4Mask | LockMask,             // CapsLock
        Mod4Mask | Mod2Mask | LockMask,  // NumLock + CapsLock
        Mod4Mask | Mod5Mask,             // ScrollLock/AltGr
        Mod4Mask | Mod2Mask | Mod5Mask,
        Mod4Mask | LockMask | Mod5Mask,
        Mod4Mask | Mod2Mask | LockMask | Mod5Mask
    };

    for (unsigned int m : masks) {
        XGrabKey(dpy_, keycode_v_, m, root, True, GrabModeAsync, GrabModeAsync);
    }

    XSync(dpy_, False);
    XSetErrorHandler(old_handler);

    registered_ = true;
    std::cout << "[GlobalHotkey] Successfully registered & grabbed Super+V on X11 root window.\n";
    return true;
}

void GlobalHotkey::unregisterHotkey() {
    if (!registered_ || !dpy_ || keycode_v_ == 0) return;

    Window root = DefaultRootWindow(dpy_);
    unsigned int masks[] = {
        Mod4Mask,
        Mod4Mask | Mod2Mask,
        Mod4Mask | LockMask,
        Mod4Mask | Mod2Mask | LockMask,
        Mod4Mask | Mod5Mask,
        Mod4Mask | Mod2Mask | Mod5Mask,
        Mod4Mask | LockMask | Mod5Mask,
        Mod4Mask | Mod2Mask | LockMask | Mod5Mask
    };

    for (unsigned int m : masks) {
        XUngrabKey(dpy_, keycode_v_, m, root);
    }
    XFlush(dpy_);
    registered_ = false;
    std::cout << "[GlobalHotkey] Unregistered Super+V hotkey.\n";
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) {
    Q_UNUSED(result);
    if (!registered_) return false;

    if (eventType == "xcb_generic_event_t") {
        auto* event = static_cast<xcb_generic_event_t*>(message);
        uint8_t type = event->response_type & ~0x80;

        if (type == XCB_KEY_PRESS) {
            auto* kev = reinterpret_cast<xcb_key_press_event_t*>(event);
            if (kev->detail == keycode_v_ && (kev->state & Mod4Mask)) {
                std::cout << "[GlobalHotkey] Super+V intercepted! Keystroke consumed from apps. Emitting triggered.\n" << std::flush;
                emit triggered();
                return true; // Consume event completely so no other app receives it!
            }
        } else if (type == XCB_KEY_RELEASE) {
            auto* kev = reinterpret_cast<xcb_key_release_event_t*>(event);
            if (kev->detail == keycode_v_) {
                return true; // Consume key release as well!
            }
        }
    }
    return false;
}
