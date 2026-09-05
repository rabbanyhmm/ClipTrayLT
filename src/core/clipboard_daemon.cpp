#include "clipboard_daemon.h"
#include "config.h"
#include <QGuiApplication>
#include <QMimeData>
#include <QImage>
#include <QBuffer>
#include <iostream>
#include <cctype>
#include <vector>
#include <string>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static bool isPasswordManagerWindowActive() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    Atom net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    Window active_win = None;

    if (XGetWindowProperty(display, DefaultRootWindow(display), net_active, 0, 1, False,
                           XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
        if (actual_type == XA_WINDOW && actual_format == 32 && nitems > 0) {
            active_win = *reinterpret_cast<Window*>(prop);
        }
        XFree(prop);
    }

    bool is_pwm = false;
    if (active_win != None) {
        XClassHint hint;
        if (XGetClassHint(display, active_win, &hint) != 0) {
            std::string res_name = hint.res_name ? hint.res_name : "";
            std::string res_class = hint.res_class ? hint.res_class : "";
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);

            std::string lower_name = res_name;
            for (auto& c : lower_name) c = std::tolower(c);
            std::string lower_class = res_class;
            for (auto& c : lower_class) c = std::tolower(c);

            const std::vector<std::string> pwm_keywords = {
                "keepass", "bitwarden", "1password", "seahorse",
                "authpass", "enpass", "lastpass", "buttercup", "passman"
            };

            for (const auto& kw : pwm_keywords) {
                if (lower_name.find(kw) != std::string::npos || lower_class.find(kw) != std::string::npos) {
                    is_pwm = true;
                    std::cout << "[ClipboardDaemon] Detected active password manager window ('"
                              << res_class << "'). Ignoring clipboard capture for privacy.\n" << std::flush;
                    break;
                }
            }
        }
    }

    XCloseDisplay(display);
    return is_pwm;
}

// Clean up X11 macro pollution so Qt symbols are unaffected
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Bool
#undef Status
#undef CursorShape

ClipboardDaemon::ClipboardDaemon(std::shared_ptr<StorageManager> storage, QObject* parent)
    : QObject(parent), storage_(storage) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, this, &ClipboardDaemon::onClipboardChanged);
    std::cout << "[ClipboardDaemon] Event-driven clipboard monitor initialized (0% idle CPU).\n";
}

void ClipboardDaemon::setSelfCopying(bool active) {
    self_copying_ = active;
}

bool ClipboardDaemon::isSelfCopying() const {
    return self_copying_;
}

void ClipboardDaemon::onClipboardChanged() {
    if (self_copying_) {
        // Ignored because this copy was initiated by our own paste action
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mime = clipboard->mimeData(QClipboard::Clipboard);
    if (!mime) return;

    if (Config::get().ignore_password_managers) {
        // 1. Check sensitive MIME types set by password managers (KeePassXC, 1Password, etc.)
        for (const QString& fmt : mime->formats()) {
            QString lower = fmt.toLower();
            if (lower.contains("password") ||
                lower.contains("secret") ||
                lower.contains("concealed") ||
                lower.contains("keepass")) {
                std::cout << "[ClipboardDaemon] Ignored copy marked as sensitive/secret (format: "
                          << fmt.toStdString() << ").\n" << std::flush;
                return;
            }
        }

        // 2. Check if active window is a known password manager application
        if (isPasswordManagerWindowActive()) {
            return;
        }
    }

    if (mime->hasText()) {
        QByteArray raw_bytes = mime->data("text/plain");
        std::string raw_str;
        if (!raw_bytes.isEmpty()) {
            raw_str.assign(raw_bytes.constData(), raw_bytes.size());
        } else {
            QString text = mime->text();
            QByteArray utf8 = text.toUtf8();
            raw_str.assign(utf8.constData(), utf8.size());
        }
        if (raw_str.empty()) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_text_copy_time_).count();
        if (raw_str == last_saved_raw_ && elapsed < 400) {
            // Rapid duplicate event from same keystroke ignored
            return;
        }
        last_saved_raw_ = raw_str;
        last_text_copy_time_ = now;

        std::string html_str;
        if (mime->hasHtml()) {
            QByteArray html_bytes = mime->data("text/html");
            if (!html_bytes.isEmpty()) {
                html_str.assign(html_bytes.constData(), html_bytes.size());
            } else {
                html_str = mime->html().toStdString();
            }
        }

        int64_t id = storage_->addItem("text", raw_str, html_str);
        std::cout << "[ClipboardDaemon] Captured copied content (ID: " << id
                  << ", Size: " << raw_str.size() << " bytes)\n" << std::flush;
        emit historyUpdated();
        return;
    }

    if (mime->hasImage()) {
        QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_image_copy_time_).count();
            if (bytes == last_saved_image_bytes_ && elapsed < 400) {
                return;
            }
            last_saved_image_bytes_ = bytes;
            last_image_copy_time_ = now;

            std::vector<uint8_t> blob(bytes.begin(), bytes.end());
            int64_t id = storage_->addItem("image", "", "", blob);
            std::cout << "[ClipboardDaemon] Captured copied image (ID: " << id << ")\n" << std::flush;
            emit historyUpdated();
            return;
        }
    }

    // Capture raw binary data streams (e.g. octet-stream, custom binary formats)
    for (const QString& fmt : mime->formats()) {
        if (fmt.contains("octet-stream") || fmt.contains("binary") || fmt.contains("raw") || fmt.contains("data")) {
            QByteArray data = mime->data(fmt);
            if (!data.isEmpty()) {
                std::string raw_str(data.constData(), data.size());
                if (raw_str == last_saved_raw_) return;
                last_saved_raw_ = raw_str;

                int64_t id = storage_->addItem("raw", raw_str, "");
                std::cout << "[ClipboardDaemon] Captured raw binary stream (ID: " << id
                          << ", Format: " << fmt.toStdString()
                          << ", Size: " << data.size() << " bytes)\n" << std::flush;
                emit historyUpdated();
                return;
            }
        }
    }
}
