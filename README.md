# ClipTray LT

A lightweight, native C++ clipboard flyout manager for Linux (X11 & Wayland).

## Features

- **Flyout Interface**: Fast popup with text snippet and image previews, favorite pinning (📌), individual deletion, and "Clear all".
- **Duplicate Prevention**: Re-copying any existing text or image moves it directly to the top of history without creating duplicate entries.
- **Fast Activation**: Opens instantly with `Super+V` or via the `cliptraylt --toggle` CLI command.
- **Hardware Paste Injection**: Automatically pastes into the active application upon selecting an item.
- **In-Memory by Default**: History is stored in RAM only by default (`save_to_disk = false`) for speed and privacy. Disk persistence can be enabled with a single command if desired.
- **Zero UI Overhead**: No cluttered settings menus in the flyout window. All options are managed through a simple CLI or config file.

## Installation

Run the installer:
```bash
git clone https://github.com/rabbanyhmm/ClipTrayLT.git
cd ClipTrayLT
./install.sh
```

The script builds the project, installs `cliptraylt` to `/usr/local/bin`, sets up hardware permissions, registers the `Super+V` shortcut, and starts the service.

## Updating

To update to the latest version at any time:
```bash
./update.sh
```

To uninstall:
```bash
./uninstall.sh
```

## CLI Commands

### View Configuration
```bash
cliptraylt config show
```

### Change Settings
Settings update live in the active daemon without needing a restart:

```bash
# Change history item limit (default: 25)
cliptraylt config set max_items 50

# Enable disk persistence across reboots (default: false / RAM only)
cliptraylt config set save_to_disk true

# Adjust flyout animation duration (in milliseconds)
cliptraylt config set appear_ms 150
cliptraylt config set hide_ms 120

# Reset all settings to defaults
cliptraylt config reset
```

Config file location: `~/.config/cliptraylt/config.ini`

### Daemon Control
```bash
cliptraylt --toggle    # Open/close the flyout
cliptraylt --show      # Open the flyout
cliptraylt --clear     # Clear all unpinned items
cliptraylt --stop      # Stop the running daemon
cliptraylt --help      # Show help
cliptraylt --version   # Show version
```
