#include <QApplication>
#include <QSystemTrayIcon>
#include <QIcon>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <filesystem>
#include <cctype>
#include <vector>
#include <string>

#include "config.h"
#include "storage.h"
#include "paste_injector.h"
#include "clipboard_daemon.h"
#include "evdev_listener.h"
#include "global_hotkey.h"
#include "flyout_window.h"

namespace fs = std::filesystem;

// Pass user session environment to root process if executed under sudo
static void setupRootDisplayEnvironment() {
    if (geteuid() != 0) return;

    const char* sudo_user = std::getenv("SUDO_USER");
    struct passwd* pw = sudo_user ? getpwnam(sudo_user) : getpwuid(getuid());
    std::string uid = pw ? std::to_string(pw->pw_uid) : "1000";

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

static std::string getSocketPath() {
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && fs::exists(xdg)) {
        return std::string(xdg) + "/cliptraylt_ipc.sock";
    }
    return "/tmp/cliptraylt_ipc_" + std::to_string(getuid()) + ".sock";
}

static std::string getLockFilePath() {
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && fs::exists(xdg)) {
        return std::string(xdg) + "/cliptraylt.lock";
    }
    return "/tmp/cliptraylt_" + std::to_string(getuid()) + ".lock";
}

static bool sendIpcCommand(const std::string& command) {
    std::vector<std::string> socket_paths = {
        getSocketPath(),
        "/tmp/cliptraylt_ipc_" + std::to_string(getuid()) + ".sock",
        "/tmp/cliptraylt_ipc_socket",
        "/tmp/simpleclipboard_ipc_socket"
    };
    for (const auto& path : socket_paths) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            std::string msg = command + "\n";
            (void)write(sock, msg.c_str(), msg.length());
            close(sock);
            return true;
        }
        close(sock);
    }
    return false;
}

static void printVersion() {
    std::cout << "ClipTray LT version 1.0.0 (Linux x86_64)\n"
              << "Lightweight clipboard flyout manager for Linux.\n"
              << "https://github.com/rabbanyhmm/ClipTrayLT\n";
}

static void printHelp(const char* prog) {
    std::string p = prog ? prog : "cliptraylt";
    size_t last_slash = p.find_last_of('/');
    if (last_slash != std::string::npos) {
        p = p.substr(last_slash + 1);
    }

    std::cout << "ClipTray LT - Lightweight Clipboard Flyout Manager for Linux\n\n"
              << "USAGE:\n"
              << "  " << p << " [OPTIONS]\n"
              << "  " << p << " config <COMMAND> [ARGS...]\n\n"
              << "OPTIONS:\n"
              << "  -h, --help                 Show this help screen and exit\n"
              << "  -v, --version              Show version information and exit\n"
              << "  -t, --toggle               Toggle clipboard flyout (via active daemon)\n"
              << "  -s, --show                 Show clipboard flyout\n"
              << "  -c, --clear                Clear unpinned clipboard history in daemon\n"
              << "      --stop                 Stop the running daemon cleanly\n"
              << "  -d, --daemon               Run clipboard manager daemon (default)\n\n"
              << "CONFIG COMMANDS:\n"
              << "  config show                Display current configuration and config path\n"
              << "  config set <KEY> <VALUE>   Update setting and notify active daemon live\n"
              << "  config reset               Reset all settings back to default values\n\n"
              << "CONFIG KEYS & VALUES:\n"
              << "  max_items <number>                 Max history capacity (e.g. 25, 50, 100)\n"
              << "  save_to_disk <true|false>          Persistent disk DB vs pure RAM (default: false / RAM only)\n"
              << "  ignore_password_managers <true|false>  Auto-ignore sensitive copies from password managers (default: true)\n"
              << "  appear_ms <number>                 Flyout entrance animation duration in ms (default: 200)\n"
              << "  hide_ms <number>                   Flyout exit animation duration in ms (default: 160)\n"
              << "  db_path <filepath>                 Custom SQLite path (or empty for default)\n\n"
              << "EXAMPLES:\n"
              << "  " << p << " config show\n"
              << "  " << p << " config set max_items 50\n"
              << "  " << p << " config set ignore_password_managers true\n"
              << "  " << p << " config set save_to_disk false\n"
              << "  " << p << " --toggle\n"
              << "  " << p << " --clear\n\n";
}

static int handleConfigCommand(int argc, char* argv[]) {
    Config::get().load();

    if (argc <= 2 || std::string(argv[2]) == "show" || std::string(argv[2]) == "get" || std::string(argv[2]) == "list") {
        std::cout << "ClipTray LT Configuration (" << Config::getConfigFilePath() << "):\n"
                  << "  [Clipboard]\n"
                  << "    max_items                : " << Config::get().max_items << "\n"
                  << "    save_to_disk             : " << (Config::get().save_to_disk ? "true (persistent SQLite on disk)" : "false (volatile in-memory only, pure RAM mode)") << "\n"
                  << "  [Privacy]\n"
                  << "    ignore_password_managers : " << (Config::get().ignore_password_managers ? "true (auto-ignore KeePass, Bitwarden, 1Password, secret hints)" : "false") << "\n"
                  << "  [Animation]\n"
                  << "    appear_ms                : " << Config::get().anim_appear_ms << " ms\n"
                  << "    hide_ms                  : " << Config::get().anim_hide_ms << " ms\n"
                  << "  [Storage]\n"
                  << "    db_path                  : " << (Config::get().db_path.empty() ? "(default auto: :memory: or ~/.local/share/cliptraylt/history.db)" : Config::get().db_path) << "\n\n";
        return 0;
    }

    std::string sub = argv[2];
    if (sub == "reset") {
        Config::get().max_items = 25;
        Config::get().save_to_disk = false;
        Config::get().ignore_password_managers = true;
        Config::get().anim_appear_ms = 200;
        Config::get().anim_hide_ms = 160;
        Config::get().db_path = "";
        Config::get().save();
        std::cout << "[ClipTray LT] Configuration reset to defaults.\n";
        if (sendIpcCommand("reload-config")) {
            std::cout << "[ClipTray LT] Active daemon notified and settings reloaded live.\n";
        }
        return 0;
    }

    if (sub == "set") {
        if (argc < 5) {
            std::cerr << "Error: 'config set' requires <KEY> and <VALUE>.\n"
                      << "Usage: cliptraylt config set <max_items|save_to_disk|ignore_password_managers|appear_ms|hide_ms|db_path> <value>\n"
                      << "Examples:\n"
                      << "  cliptraylt config set max_items 50\n"
                      << "  cliptraylt config set ignore_password_managers true\n"
                      << "  cliptraylt config set save_to_disk false\n";
            return 1;
        }
        std::string key = argv[3];
        std::string val = argv[4];

        if (key == "max_items") {
            try {
                int n = std::stoi(val);
                if (n < 1) {
                    std::cerr << "Error: max_items must be at least 1.\n";
                    return 1;
                }
                Config::get().max_items = n;
            } catch (...) {
                std::cerr << "Error: Invalid number '" << val << "' for max_items.\n";
                return 1;
            }
        } else if (key == "save_to_disk") {
            std::string lower_val = val;
            for (auto& c : lower_val) c = std::tolower(c);
            if (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on") {
                Config::get().save_to_disk = true;
            } else if (lower_val == "false" || lower_val == "0" || lower_val == "no" || lower_val == "off") {
                Config::get().save_to_disk = false;
            } else {
                std::cerr << "Error: save_to_disk must be 'true' or 'false'.\n";
                return 1;
            }
        } else if (key == "ignore_password_managers" || key == "ignore_passwords") {
            std::string lower_val = val;
            for (auto& c : lower_val) c = std::tolower(c);
            if (lower_val == "true" || lower_val == "1" || lower_val == "yes" || lower_val == "on") {
                Config::get().ignore_password_managers = true;
            } else if (lower_val == "false" || lower_val == "0" || lower_val == "no" || lower_val == "off") {
                Config::get().ignore_password_managers = false;
            } else {
                std::cerr << "Error: " << key << " must be 'true' or 'false'.\n";
                return 1;
            }
        } else if (key == "appear_ms") {
            try {
                int n = std::stoi(val);
                if (n < 0) {
                    std::cerr << "Error: appear_ms cannot be negative.\n";
                    return 1;
                }
                Config::get().anim_appear_ms = n;
            } catch (...) {
                std::cerr << "Error: Invalid number '" << val << "' for appear_ms.\n";
                return 1;
            }
        } else if (key == "hide_ms") {
            try {
                int n = std::stoi(val);
                if (n < 0) {
                    std::cerr << "Error: hide_ms cannot be negative.\n";
                    return 1;
                }
                Config::get().anim_hide_ms = n;
            } catch (...) {
                std::cerr << "Error: Invalid number '" << val << "' for hide_ms.\n";
                return 1;
            }
        } else if (key == "db_path") {
            Config::get().db_path = val;
        } else {
            std::cerr << "Error: Unknown config key '" << key << "'.\n"
                      << "Available keys: max_items, save_to_disk, ignore_password_managers, appear_ms, hide_ms, db_path\n";
            return 1;
        }

        Config::get().save();
        std::cout << "[ClipTray LT] Successfully set '" << key << "' = " << val << "\n"
                  << "[ClipTray LT] Configuration saved to " << Config::getConfigFilePath() << "\n";

        if (sendIpcCommand("reload-config")) {
            std::cout << "[ClipTray LT] Active daemon notified and settings updated live.\n";
        }
        return 0;
    }

    std::cerr << "Error: Unknown config subcommand '" << sub << "'.\n"
              << "Available subcommands: show, set, reset\n";
    return 1;
}

static QIcon createTrayIcon() {
    QIcon icon(":/icons/app_icon.png");
    if (!icon.isNull()) {
        return icon;
    }
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
    // 1. Process instant CLI arguments before initializing GUI system
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "-h" || arg1 == "--help" || arg1 == "help") {
            printHelp(argv[0]);
            return 0;
        }
        if (arg1 == "-v" || arg1 == "--version" || arg1 == "version") {
            printVersion();
            return 0;
        }
        if (arg1 == "config") {
            return handleConfigCommand(argc, argv);
        }
        if (arg1 == "-t" || arg1 == "--toggle" || arg1 == "toggle") {
            if (sendIpcCommand("toggle")) {
                std::cout << "[ClipTray LT] Flyout toggled.\n";
                return 0;
            } else {
                std::cerr << "[ClipTray LT] Daemon is not currently running.\n";
                return 1;
            }
        }
        if (arg1 == "-s" || arg1 == "--show" || arg1 == "show") {
            if (sendIpcCommand("show")) {
                std::cout << "[ClipTray LT] Flyout shown.\n";
                return 0;
            } else {
                std::cerr << "[ClipTray LT] Daemon is not currently running.\n";
                return 1;
            }
        }
        if (arg1 == "-c" || arg1 == "--clear" || arg1 == "clear") {
            if (sendIpcCommand("clear")) {
                std::cout << "[ClipTray LT] Clipboard history cleared.\n";
                return 0;
            } else {
                std::cerr << "[ClipTray LT] Daemon is not currently running.\n";
                return 1;
            }
        }
        if (arg1 == "--stop" || arg1 == "stop") {
            if (sendIpcCommand("stop")) {
                std::cout << "[ClipTray LT] Daemon stopped.\n";
                return 0;
            } else {
                std::cerr << "[ClipTray LT] Daemon is not currently running.\n";
                return 1;
            }
        }
        if (arg1 != "-d" && arg1 != "--daemon") {
            std::cerr << "Error: Unknown argument '" << arg1 << "'.\n"
                      << "Run '" << (argv[0] ? argv[0] : "cliptraylt") << " --help' for usage instructions.\n";
            return 1;
        }
    }

    // Prefer xcb for positioning, fallback to wayland
    setenv("QT_QPA_PLATFORM", "xcb", 1);

    setupRootDisplayEnvironment();

    std::string lock_file = getLockFilePath();
    int lock_fd = open(lock_file.c_str(), O_CREAT | O_RDWR, 0666);
    if (lock_fd >= 0) {
        fchmod(lock_fd, 0666);
    }
    if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        if (sendIpcCommand("toggle")) {
            std::cout << "[ClipTray LT] Existing daemon instance triggered via IPC. Exiting duplicate process.\n";
            return 0;
        }
        std::cout << "[ClipTray LT] Another instance is already running. Exiting.\n";
        return 0;
    }

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(createTrayIcon());

    std::cout << "===================================================\n";
    std::cout << "                 ClipTray LT                       \n";
    std::cout << "===================================================\n";

    // Load lightweight configuration from INI or defaults
    Config::get().load();

    // Initialize Core Modules
    auto storage = std::make_shared<StorageManager>();
    auto paste_injector = std::make_shared<PasteInjector>();
    auto clip_daemon = std::make_shared<ClipboardDaemon>(storage);

    // Exactly ONE single Flyout window in memory (pre-warmed for zero-delay display)
    auto* window = new FlyoutWindow(storage, paste_injector, clip_daemon);

    // Hardware Evdev Mouse & Escape Sniffer
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
        std::cerr << "[!] Note: Hardware sniffer requires read access to /dev/input/event*.\n";
    }

    // Local IPC socket server
    std::string sock_path = getSocketPath();
    unlink(sock_path.c_str());
    QLocalServer::removeServer(QString::fromStdString(sock_path));

    auto* ipc = new QLocalServer(&app);
    if (ipc->listen(QString::fromStdString(sock_path))) {
        chmod(sock_path.c_str(), 0666);
        unlink("/tmp/cliptraylt_ipc_socket");
        (void)symlink(sock_path.c_str(), "/tmp/cliptraylt_ipc_socket");

        QObject::connect(ipc, &QLocalServer::newConnection, [ipc, window, storage, &app]() {
            QLocalSocket* sock = ipc->nextPendingConnection();
            if (!sock) return;
            QObject::connect(sock, &QLocalSocket::readyRead, [sock, window, storage, &app]() {
                QByteArray msg = sock->readAll();
                if (msg.contains("toggle")) {
                    window->toggleFlyout();
                } else if (msg.contains("show")) {
                    window->showFlyout();
                } else if (msg.contains("clear")) {
                    storage->clearUnpinned();
                    window->reloadHistory();
                    std::cout << "[Daemon] Unpinned history cleared via IPC.\n";
                } else if (msg.contains("stop")) {
                    std::cout << "[Daemon] Clean stop requested via IPC. Exiting.\n";
                    app.quit();
                } else if (msg.contains("reload-config")) {
                    Config::get().load();
                    storage->enforceMaxItems();
                    window->reloadHistory();
                    std::cout << "[Daemon] Configuration reloaded live via IPC: max_items="
                              << Config::get().max_items << ", save_to_disk="
                              << (Config::get().save_to_disk ? "true" : "false")
                              << ", ignore_passwords="
                              << (Config::get().ignore_password_managers ? "true" : "false") << "\n";
                }
            });
        });
    }

    // System Tray
    auto* tray_icon = new QSystemTrayIcon(createTrayIcon(), &app);
    auto* tray_menu = new QMenu();

    auto* show_action = tray_menu->addAction("Show Clipboard (Super+V)");
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

    std::cout << "[Ready] Monitoring clipboard (Max " << Config::get().max_items << " items, "
              << (Config::get().save_to_disk ? "persistent on disk" : "in-memory RAM only") << ").\n";
    std::cout << "[Ready] Press SUPER + V to toggle flyout.\n";
    std::cout << "[Ready] Click anywhere outside to dismiss.\n";

    return app.exec();
}
