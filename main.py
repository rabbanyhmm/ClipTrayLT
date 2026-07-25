import sys
import os
import logging
from PySide6.QtWidgets import QApplication, QSystemTrayIcon, QMenu
from PySide6.QtCore import Qt
from PySide6.QtGui import QIcon, QAction, QPixmap, QPainter, QColor
from PySide6.QtNetwork import QLocalServer, QLocalSocket

from clip_engine.storage import StorageManager
from clip_engine.clipboard_monitor import ClipboardMonitor
from clip_engine.hotkey_listener import HotkeyListener
from clip_engine.evdev_listener import EvdevListener
from clip_engine.paste_injector import PasteInjector
from clip_ui.main_window import ClipboardFlyoutWindow

# Force X11/Xwayland backend for client positioning, fall back gracefully to Wayland if xcb-cursor is missing
os.environ["QT_QPA_PLATFORM"] = "xcb;wayland"

# Preserve display environment when running under sudo
if os.geteuid() == 0:
    sudo_user = os.environ.get("SUDO_USER", "rabbany")
    user_uid = "1000"
    if "XDG_RUNTIME_DIR" not in os.environ and os.path.exists(f"/run/user/{user_uid}"):
        os.environ["XDG_RUNTIME_DIR"] = f"/run/user/{user_uid}"
    if "WAYLAND_DISPLAY" not in os.environ and os.path.exists(f"/run/user/{user_uid}/wayland-0"):
        os.environ["WAYLAND_DISPLAY"] = "wayland-0"
    if "DISPLAY" not in os.environ:
        os.environ["DISPLAY"] = ":0"
    if "XAUTHORITY" not in os.environ and os.path.exists(f"/home/{sudo_user}/.Xauthority"):
        os.environ["XAUTHORITY"] = f"/home/{sudo_user}/.Xauthority"

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
logger = logging.getLogger("WinClipboard")

SOCKET_NAME = f"win_clipboard_ipc_socket_{os.getuid() if hasattr(os, 'getuid') else 0}"

def send_toggle_ipc() -> bool:
    socket = QLocalSocket()
    socket.connectToServer(SOCKET_NAME)
    if socket.waitForConnected(500):
        socket.write(b"toggle\n")
        socket.waitForBytesWritten(500)
        socket.disconnectFromServer()
        return True
    return False

def create_tray_icon_pixmap() -> QPixmap:
    pixmap = QPixmap(32, 32)
    pixmap.fill(Qt.transparent)
    painter = QPainter(pixmap)
    painter.setRenderHint(QPainter.Antialiasing)

    painter.setBrush(QColor("#0078d4"))
    painter.setPen(Qt.NoPen)
    painter.drawRoundedRect(6, 6, 20, 24, 4, 4)

    painter.setBrush(QColor("#ffffff"))
    painter.drawRoundedRect(11, 3, 10, 5, 2, 2)

    painter.setPen(QColor("#ffffff"))
    painter.drawLine(10, 14, 22, 14)
    painter.drawLine(10, 18, 22, 18)
    painter.drawLine(10, 22, 18, 22)
    painter.end()

    return pixmap

def main():
    app = QApplication(sys.argv)
    app.setQuitOnLastWindowClosed(False)
    app.setApplicationName("WinClipboard")

    # Connect to verify if another instance is already running
    socket = QLocalSocket()
    socket.connectToServer(SOCKET_NAME)
    if socket.waitForConnected(300):
        # Already running. Toggle window if arguments passed
        if "--toggle" in sys.argv or len(sys.argv) > 1:
            socket.write(b"toggle\n")
            socket.waitForBytesWritten(300)
        else:
            logger.info("WinClipboard is already running.")
        socket.disconnectFromServer()
        sys.exit(0)

    # Re-bind IPC socket
    QLocalServer.removeServer(SOCKET_NAME)

    # 1. Initialize Storage Manager
    storage = StorageManager()

    # 2. Initialize Low-level Clipboard Monitor
    clipboard = app.clipboard()
    monitor = ClipboardMonitor(storage=storage, clipboard=clipboard)

    # 3. Initialize Paste Injector
    paste_injector = PasteInjector()

    # 4. Initialize UI Flyout
    flyout = ClipboardFlyoutWindow(storage_manager=storage)

    # Connect low-level monitor item_added signal to update flyout history in real time!
    monitor.item_added.connect(lambda item: flyout.reload_history())

    # 5. Handle item selection from Flyout UI
    def on_item_selected(args):
        item_data, was_active = args
        monitor.set_clipboard_content(item_data)
        delay = 0.06 if was_active else 0.01
        paste_injector.paste(delay=delay)

    def on_symbol_selected(args):
        symbol, was_active = args
        monitor.set_clipboard_content({'content_type': 'text', 'text_content': symbol})
        delay = 0.06 if was_active else 0.01
        paste_injector.paste(delay=delay)

    flyout.item_selected.connect(on_item_selected)
    flyout.symbol_selected.connect(on_symbol_selected)

    # 6. Initialize IPC Local Server for Linux System Shortcuts
    ipc_server = QLocalServer()
    if ipc_server.listen(SOCKET_NAME):
        def on_new_ipc_connection():
            client = ipc_server.nextPendingConnection()
            if client:
                client.readyRead.connect(lambda: (flyout.toggle_window(), client.deleteLater()))
        ipc_server.newConnection.connect(on_new_ipc_connection)
        logger.info("IPC Local Server active for Linux desktop shortcuts.")

    # 7. Triple Fail-Safe Hotkey Architecture
    def on_hotkey_triggered():
        logger.info("Global shortcut triggered! Toggling flyout window.")
        flyout.toggle_window()

    # Backend A: Linux Kernel evdev hardware sniffer
    evdev_listener = EvdevListener()
    evdev_listener.toggle_signal.connect(on_hotkey_triggered, Qt.QueuedConnection)
    evdev_listener.start()

    # Backend B: User-space pynput hook (Only started if evdev is inactive to prevent duplicate triggers)
    hotkey = None
    if not getattr(evdev_listener, '_devices', []):
        hotkey = HotkeyListener()
        hotkey.toggle_signal.connect(on_hotkey_triggered, Qt.QueuedConnection)
        hotkey.start()
        logger.info("HotkeyListener (pynput fallback) active.")

    # 8. System Tray Icon Setup
    tray_icon = QSystemTrayIcon(QIcon(create_tray_icon_pixmap()), app)
    tray_menu = QMenu()

    show_action = QAction("Open Clipboard (Super+V)", app)
    show_action.triggered.connect(flyout.show_near_cursor)
    tray_menu.addAction(show_action)

    clear_action = QAction("Clear Unpinned History", app)
    clear_action.triggered.connect(flyout.on_clear_unpinned)
    tray_menu.addAction(clear_action)

    tray_menu.addSeparator()

    quit_action = QAction("Exit", app)
    quit_action.triggered.connect(lambda: (
        evdev_listener.stop(),
        hotkey.stop() if hotkey else None,
        paste_injector.cleanup(),
        ipc_server.close(),
        app.quit()
    ))
    tray_menu.addAction(quit_action)

    tray_icon.setContextMenu(tray_menu)
    tray_icon.setToolTip("Windows Clipboard Flyout (Super+V)")
    tray_icon.activated.connect(lambda reason: flyout.show_near_cursor() if reason == QSystemTrayIcon.Trigger else None)
    tray_icon.show()

    logger.info("Windows Clipboard Flyout started successfully. Opening window...")
    flyout.show_near_cursor()
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
