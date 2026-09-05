<p align="center">
  <img src="assets/banner.png" alt="ClipTray LT" width="100%">
</p>

<h1 align="center">ClipTray LT</h1>

<p align="center">
  <strong>Fast, lightweight clipboard flyout manager for Linux.</strong>
</p>

<p align="center">
  <a href="https://github.com/rabbanyhmm/ClipTrayLT/actions/workflows/ci.yml"><img src="https://github.com/rabbanyhmm/ClipTrayLT/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="https://github.com/rabbanyhmm/ClipTrayLT"><img src="https://img.shields.io/badge/Platform-Linux-blue.svg" alt="Platform"></a>
  <a href="https://github.com/rabbanyhmm/ClipTrayLT"><img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B" alt="C++20"></a>
  <a href="https://github.com/rabbanyhmm/ClipTrayLT"><img src="https://img.shields.io/badge/Qt-6-41CD52?logo=qt" alt="Qt6"></a>
  <a href="https://github.com/rabbanyhmm/ClipTrayLT"><img src="https://img.shields.io/badge/Display-X11%20%7C%20Wayland-informational" alt="Display Server"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

---

ClipTray LT is an event-driven clipboard history flyout for Linux. It operates with **0% idle CPU**, stores clips in **volatile RAM by default**, and enables instant search, keyboard navigation, and direct pasting into active applications.

## Features

- **Instant Search**: Type immediately upon opening to filter text and snippets in real time.
- **Full Keyboard Navigation**: Move with arrow keys, press Enter to paste, or use quick keys `1`–`9`.
- **Direct Application Pasting**: Pastes selected clips directly into the currently active target window.
- **Privacy First**: Automatically detects and ignores passwords copied from KeePassXC, Bitwarden, 1Password, etc.
- **Volatile RAM Mode**: Pure in-memory operation by default—no clips are written to disk unless explicitly configured.
- **Pinning**: Pin favorite clips to keep them permanently across restarts.
- **Zero Idle CPU**: 100% event-driven architecture using Linux kernel `uinput`, `libevdev`, and Qt6.

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Super + V` | Open or close clipboard flyout |
| `Up` / `Down` | Navigate through history cards |
| `Enter` | Paste selected item into active application |
| `1` – `9` (or `Alt + 1`–`9`) | Quick paste items 1 through 9 |
| `Esc` | Clear search query, or close the flyout |
| *Click Outside* | Dismiss flyout |

## Installation

Run the automated installer:

```bash
git clone https://github.com/rabbanyhmm/ClipTrayLT.git
cd ClipTrayLT
./install.sh
```

The installer builds the native binary, sets up persistent udev permissions (no root/`sudo` needed at runtime), configures the `Super+V` shortcut, and registers a background user service.

### Updating

```bash
./update.sh
```

### Uninstallation

```bash
./uninstall.sh
```

## Configuration

ClipTray LT keeps the user interface clean with no settings clutter. All configuration is managed via CLI commands and reloaded live by the daemon.

### Quick Commands

```bash
# View configuration and status
cliptraylt config show

# Set history capacity (default: 25)
cliptraylt config set max_items 50

# Toggle persistent disk storage vs in-memory RAM (default: false)
cliptraylt config set save_to_disk true

# Toggle password manager exclusion (default: true)
cliptraylt config set ignore_password_managers true

# Adjust flyout animation durations (milliseconds)
cliptraylt config set appear_ms 200
cliptraylt config set hide_ms 160

# Reset all settings to factory defaults
cliptraylt config reset
```

### Settings Reference

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `max_items` | Integer | `25` | Maximum number of clipboard items retained in history |
| `save_to_disk` | Boolean | `false` | `false` = pure RAM memory mode; `true` = persistent SQLite on disk |
| `ignore_password_managers` | Boolean | `true` | Auto-detect and ignore copies from Bitwarden, KeePassXC, 1Password, etc. |
| `appear_ms` | Integer | `200` | Flyout entrance animation duration (ms) |
| `hide_ms` | Integer | `160` | Flyout exit animation duration (ms) |
| `db_path` | String | `""` | Custom SQLite database file path (optional) |

### Controls

```bash
cliptraylt --toggle    # Toggle flyout visibility via IPC
cliptraylt --clear     # Clear unpinned items
cliptraylt --stop      # Stop background daemon cleanly
cliptraylt --help      # Show help and CLI options
```

## System Requirements

- **Operating System**: Linux (Ubuntu, Debian, Fedora, Arch, etc.)
- **Display Server**: X11 or Wayland (GNOME, KDE Plasma, XFCE, etc.)
- **Dependencies**: `cmake`, `g++` (C++20), `qt6-base-dev`, `libevdev-dev`, `libsqlite3-dev`, `libatspi2.0-dev` (automatically installed by `install.sh` on Debian/Ubuntu).

## License

This project is licensed under the [MIT License](LICENSE).
