# SimpleClipboard

A high-performance, Windows 11 style Clipboard History, Emoji, and Symbol Flyout for Linux desktops (compatible with both **Wayland** and **X11** display servers).

SimpleClipboard mimics the exact user experience, styling, and behavior of the Windows `Win + V` clipboard flyout, engineered natively in Python and PySide6.

---

## ✨ Features

- 🎨 **Windows 11 Fluent Dark Design**: Custom card overlays, rounded timestamp badges, emoji grids, symbol categories, and tab animations matching the official Windows design.
- 🎯 **Dual-Mode Placement Engine**:
  - **Active Input Focused**: Spawns directly adjacent to the active text typing caret.
  - **No Input Focused**: Anchors to the bottom-right corner of the primary monitor work area.
- ⚡ **Kernel-Level Hardware Paste Emulation (`uinput`)**: Bypasses Wayland's security/input isolation. Emulates a physical USB keyboard driver at the Linux kernel level to inject `Ctrl + V` instantaneously into any browser, IDE, or terminal.
- ⌨️ **Keyboard Navigation**: Press `Up` / `Down` arrows to navigate the history cards and press `Enter` to copy and paste instantly. Works seamlessly even while typing inside the search bar.
- 📋 **Bit-Exact MIME Preservation**: SQLite database preserves raw MIME data streams (text, HTML formatting, image binaries, custom streams) exactly as copied.
- ✋ **Fluid System Move**: Frameless window dragging handles position offsets natively.
- 🚀 **Unified Hotkey Deduplication**: Triple-redundant shortcut listening (`evdev` kernel sniffer, native GNOME shortcut sockets, user-space `pynput` hook) runs cleanly without duplicate window triggers.

---

## 🛠️ Installation & Requirements

### 1. System Dependencies
Install core packages required for Qt X11 support and absolute positioning:
```bash
sudo apt-get update
sudo apt-get install -y libxcb-cursor0 xdotool
```

### 2. Python Environment & Dependencies
Clone the repository and configure the virtual environment:
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

---

## 🚀 How to Run

### Setup Device Permissions (Recommended)
Since hardware paste injection and keyboard sniffing interface directly with `/dev/uinput` and `/dev/input/`, you can use the launcher script to configure standard user permissions once (asking for sudo) and start the application:

```bash
chmod +x run.sh
./run.sh
```

---

## ⌨️ Global Shortcut Mapping (`Super + V`)

The application listens natively for `Super + V` (Windows Key + V). You can also bind this shortcut to GNOME system keyboard settings:

1. Go to **Settings** -> **Keyboard** -> **View and Customise Shortcuts** -> **Custom Shortcuts**.
2. Add a new shortcut:
   - **Name**: `Toggle Clipboard`
   - **Command**: `/path/to/SimpleClipboard/.venv/bin/python /path/to/SimpleClipboard/main.py --toggle`
   - **Shortcut**: `Super + V`

---

## 🏗️ Architecture

```
                                 [ evdev Sniffer ]
                                         |
                                         v
 [ Ctrl+C Copy ] ---> [ ClipboardMonitor ] ---> [ SQLite Storage ]
                                                        |
                                                        v
 [ Super+V Press ] -> [ CaretDetector ] --------> [ Flyout UI ]
                                                        |  (Enter / Click Card)
                                                        v
                                                [ PasteInjector ] ---> [ Direct Kernel Emulation ]
```

- **`clip_engine/`**: Keyboard sniffers, caret trackers, and direct hardware keyboard drivers.
- **`clip_ui/`**: Qt 6 components, navigation bars, and styling.
- **`main.py`**: Local local-sockets IPC server and system tray initialization.

---

## 📄 License
This project is licensed under the MIT License.
