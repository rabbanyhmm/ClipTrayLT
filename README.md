# ClipTray LT (Windows 10 Edition)

A high-performance, native C++ implementation of the **Windows 10 Clipboard Flyout (`Win + V`)** for Ubuntu and Linux desktops (Wayland and X11).

[![Repository](https://img.shields.io/badge/GitHub-ClipTrayLT-blue)](https://github.com/rabbanyhmm/ClipTrayLT)
[![Language](https://img.shields.io/badge/Language-C%2B%2B20-green)](https://en.cppreference.com/)
[![Framework](https://img.shields.io/badge/GUI-Qt6-brightgreen)](https://www.qt.io/)

---

## 🚀 Features

- **Exact Windows 10 Look & Feel**:
  - Dark acrylic flyout style (`#1f1f1f`) with native drop shadow.
  - "Clipboard" header with "Clear all" action.
  - History cards with text snippet and image previews.
  - Pinning (📌) support to preserve favorites against "Clear all".
  - Individual item delete (✕).
  - Smooth 60fps entrance/exit fade & slide animations.
  - Smooth kinetic scroll bar with customized acrylic styling.
- **Strict No-Duplicate Rule**:
  - Re-copying an existing item (text or image) updates its timestamp and brings it directly to the top of clipboard history without creating duplicate entries.
- **Zero-Delay Microsecond Activation**:
  - Pre-warmed in-memory window (`show()` / `hide()` executes in < 0.5ms).
  - Background kernel `libevdev` key sniffer for `Super + V` (runs with root permissions).
  - Global X11 / Xwayland grabber consumes the keystroke before apps can misinterpret it.
  - Double-redundancy: microsecond IPC socket (`--toggle`) for instant desktop shortcut integration.
- **Kernel-Level Hardware Paste Injection**:
  - Direct `/dev/uinput` virtual USB keyboard injection (`Ctrl + V`).
  - Hides the flyout instantly on click/Enter and immediately pastes into the target application.
- **Ultra-Lightweight Storage (Default: In-Memory / RAM-only)**:
  - **Volatile In-Memory (Default)**: By default (`save_to_disk = false`), clipboard history resides purely in volatile RAM (`:memory:`) for maximum privacy, zero disk wear, and sub-microsecond speed. History is wiped clean on exit.
  - **Persistent Disk Mode**: Can be toggled with one CLI command (`cliptraylt config set save_to_disk true`), utilizing WAL mode and 30MB memory-mapped I/O (`mmap`).

---

## ⚙️ CLI & Configuration Commands

ClipTray LT features a clean, human-designed CLI interface for inspecting and updating settings live without restarting the daemon:

### View Configuration
```bash
cliptraylt config show
```

### Change Settings Live
```bash
# Increase clipboard history limit to 50 items
cliptraylt config set max_items 50

# Enable persistent disk storage (or set to false for pure RAM mode)
cliptraylt config set save_to_disk true

# Adjust flyout animation duration (in milliseconds)
cliptraylt config set appear_ms 150
cliptraylt config set hide_ms 120

# Reset all settings to factory defaults
cliptraylt config reset
```

### Control Active Daemon
```bash
# Toggle flyout window open/close
cliptraylt --toggle

# Show flyout
cliptraylt --show

# Clear unpinned history
cliptraylt --clear

# Stop running daemon cleanly
cliptraylt --stop
```

Configuration file is stored at `~/.config/cliptraylt/config.ini`.

---

## 🛠️ Build & Run

### 1. Requirements (Ubuntu / Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake qt6-base-dev libevdev-dev libsqlite3-dev libatspi-dev
```

### 2. Compile
```bash
cmake -B build -S .
cmake --build build
```

### 3. Run Daemon
```bash
./run.sh
```

### 4. Register GNOME Desktop Shortcut (`Super + V`)
```bash
./setup_shortcut.sh
```
