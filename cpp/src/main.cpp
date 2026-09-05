#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPixmap>
#include <QLocalServer>
#include <QLocalSocket>
#include <QCursor>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <filesystem>

#include "storage.h"
#include "paste_injector.h"
#include "clipboard_daemon.h"
#include "evdev_listener.h"
#include "global_hotkey.h"
#include "flyout_window.h"

namespace fs = std::filesystem;

// Pass user session environment to root process
static void setupRootDisplayEnvironment() {
    if (geteuid() != 0) return;

    const char* sudo_user = std::getenv("SUDO_USER");
    std::string user_name = sudo_user ? sudo_user : "rabbany";
    std::string uid = "1000";

    struct passwd* pw = getpwnam(user_name.c_str());
    if (pw) {
        uid = std::to_string(pw->pw_uid);
    }

    if (!std::getenv("XDG_RUNTIME_DIR")) {
        std::string run_user = "/run/user/" + uid;
        if (fs::exists(run_user)) {
            setenv("XDG_RUNTIME_DIR", run_user.c_str(), 1);
        }
    }

    if (!std::getenv("DBUS_SESSION_BUS_ADDRESS")) {
        std::string dbus_sock = "/run/user/" + uid + "/bus";
        if (fs::exists(dbus_sock)) {
            std::string dbus_addr = "unix:path=" + dbus_sock;
            setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr.c_str(), 1);
        }
    }

    if (!std::getenv("WAYLAND_DISPLAY")) {
        std::string wayland_sock = "/run/user/" + uid + "/wayland-0";
        if (fs::exists(wayland_sock)) {
            setenv("WAYLAND_DISPLAY", "wayland-0", 1);
        }
    }

    if (!std::getenv("DISPLAY")) {
        setenv("DISPLAY", ":0", 1);
    }

    if (!std::getenv("XAUTHORITY")) {
        if (pw && pw->pw_dir) {
            std::string xauth = std::string(pw->pw_dir) + "/.Xauthority";
            if (fs::exists(xauth)) {
                setenv("XAUTHORITY", xauth.c_str(), 1);
            }
        }
    }
}

static QIcon createTrayIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setBrush(QColor("#0078d4"));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(6, 6, 20, 24, 3, 3);

    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(11, 3, 10, 5, 2, 2);

    painter.setPen(QPen(QColor("#ffffff"), 1.5));
    painter.drawLine(10, 13, 22, 13);
    painter.drawLine(10, 17, 22, 17);
    painter.drawLine(10, 21, 18, 21);

    return QIcon(pixmap);
}

int main(int argc, char *argv[]) {
    // Prefer xcb for positioning, fallback to wayland
    setenv("QT_QPA_PLATFORM", "xcb", 1);

    setupRootDisplayEnvironment();

    const QString socket_name = "simpleclipboard_ipc_socket";

    // Single-instance process lock
    int lock_fd = open("/tmp/simpleclipboard_instance.lock", O_CREAT | O_RDWR, 0666);
    if (lock_fd >= 0) {
        fchmod(lock_fd, 0666);
    }
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        QLocalSocket socket;
        socket.connectToServer(socket_name);
        if (socket.waitForConnected(500)) {
            socket.write("toggle\n");
            socket.waitForBytesWritten(500);
            socket.disconnectFromServer();
            std::cout << "[SimpleClipboard] Existing daemon instance triggered via IPC. Exiting duplicate process.\n";
            return 0;
        }
        std::cout << "[SimpleClipboard] Another instance is already running. Exiting.\n";
        return 0;
    }

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    std::cout << "===================================================\n";
    std::cout << "    SimpleClipboard Native (Windows 10 Edition)    \n";
    std::cout << "===================================================\n";

    // Initialize Core Modules
    auto storage = std::make_shared<StorageManager>();
    auto paste_injector = std::make_shared<PasteInjector>();
    auto clip_daemon = std::make_shared<ClipboardDaemon>(storage);

    // Exactly ONE single Flyout window in memory (pre-warmed for zero-delay display)
    auto* window = new FlyoutWindow(storage, paste_injector, clip_daemon);

    // Hardware Evdev Mouse & Escape Sniffer (Root level)
    auto* evdev = new EvdevListener(&app);

    // Global X11/Xwayland Hotkey Grabber:
    // Intercepts and consumes Super+V directly from the display server so that active apps
    // (such as Discord) never receive the key event and will NEVER auto-paste!
    auto* global_hotkey = new GlobalHotkey(&app);
    bool hotkey_registered = global_hotkey->registerHotkey();
    if (hotkey_registered) {
        app.installNativeEventFilter(global_hotkey);
        QObject::connect(global_hotkey, &GlobalHotkey::triggered, window, [window]() {
            window->showFlyout();
        }, Qt::QueuedConnection);
        std::cout << "[✓] Global Super+V hotkey grabber ACTIVE (Keystroke consumed from apps).\n";
    }

    // Hardware Evdev fallback trigger (ensures flyout ALWAYS opens regardless of session type)
    QObject::connect(evdev, &EvdevListener::hotkeyTriggered, window, [window]() {
        window->showFlyout();
    }, Qt::QueuedConnection);

    // Keyboard Escape dismiss
    QObject::connect(evdev, &EvdevListener::escapePressed, window, [window]() {
        if (window->isVisible()) {
            window->hideFlyout();
        }
    }, Qt::QueuedConnection);

    // Mouse outside click detection: immediately hides window if clicked outside
    QObject::connect(evdev, &EvdevListener::globalMousePressed, window, [window]() {
        if (window->isVisible()) {
            window->handleGlobalClick();
        }
    }, Qt::QueuedConnection);

    if (evdev->start()) {
        std::cout << "[✓] Mouse Click & Escape Sniffer ACTIVE.\n";
    } else {
        std::cerr << "[!] Could not start hardware sniffer directly. Are you running with root / sudo?\n";
    }

    // Local IPC socket server
    auto* ipc = new QLocalServer(&app);
    QLocalServer::removeServer(socket_name);
    if (ipc->listen(socket_name)) {
        chmod(("/tmp/" + socket_name.toStdString()).c_str(), 0666);
        QObject::connect(ipc, &QLocalServer::newConnection, [ipc, window]() {
            QLocalSocket* sock = ipc->nextPendingConnection();
            if (!sock) return;
            QObject::connect(sock, &QLocalSocket::readyRead, [sock, window]() {
                QByteArray msg = sock->readAll();
                if (msg.contains("toggle") || msg.contains("show")) {
                    window->showFlyout();
                }
            });
        });
    }

    // System Tray
    auto* tray_icon = new QSystemTrayIcon(createTrayIcon(), &app);
    auto* tray_menu = new QMenu();

    auto* show_action = tray_menu->addAction("Show Clipboard (Win+V)");
    QObject::connect(show_action, &QAction::triggered, window, &FlyoutWindow::showFlyout);

    auto* clear_action = tray_menu->addAction("Clear History");
    QObject::connect(clear_action, &QAction::triggered, [storage, window]() {
        storage->clearUnpinned();
        window->reloadHistory();
    });

    tray_menu->addSeparator();

    auto* quit_action = tray_menu->addAction("Exit");
    QObject::connect(quit_action, &QAction::triggered, &app, &QApplication::quit);

    tray_icon->setContextMenu(tray_menu);
    tray_icon->show();

    std::cout << "[Ready] Monitoring all system copies (Max 25 items saved).\n";
    std::cout << "[Ready] Press SUPER + V to toggle flyout.\n";
    std::cout << "[Ready] Click anywhere outside to dismiss.\n";

    return app.exec();
}
