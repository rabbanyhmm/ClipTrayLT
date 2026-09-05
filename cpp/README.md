# SimpleClipboard Native (C++ / Windows 10 Edition)

A high-performance, native C++ implementation of the **Windows 10 Clipboard Flyout (`Win + V`)** for Ubuntu and Linux desktops (Wayland and X11).

## 🚀 Features

- **Windows 10 Exact Look & Feel**:
  - Dark acrylic flyout style (`#1f1f1f`) with drop shadow.
  - "Clipboard" header and "Clear all" action.
  - History cards with text snippet and image previews.
  - Pinning (📌) support to preserve favorites against "Clear all".
  - Individual item delete (✕).
- **Sub-Millisecond Zero-Delay Pop-up**:
  - Pre-warmed in-memory window (`show()` / `hide()` executes in < 0.5ms).
  - Background kernel `libevdev` key sniffer for `Super + V` (runs with root permissions).
  - Double-redundancy: also includes a microsecond IPC socket (`--toggle`) compatible with GNOME custom shortcuts.
- **Kernel-Level Hardware Paste Injection**:
  - Direct `/dev/uinput` virtual USB keyboard injection (`Ctrl + V`).
  - Hides the window instantly on click/Enter and immediately pastes into the target application.
- **Lightweight & Efficient**:
  - 160 KB compiled binary.
  - Microsecond SQLite3 storage.

## 🛠️ Build & Run

### 1. Build Requirements
Installed via apt:
```bash
sudo apt-get install -y build-essential cmake qt6-base-dev libevdev-dev libsqlite3-dev
```

### 2. Compile
```bash
cmake -B build -S .
cmake --build build
```

### 3. Run with Root Privileges
```bash
./run.sh --root
```

### 4. Register GNOME Desktop Shortcut (`Super + V`)
```bash
./setup_shortcut.sh
```
