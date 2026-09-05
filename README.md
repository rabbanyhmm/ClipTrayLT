<p align="center">
  <img src="assets/banner.png" alt="ClipTray LT" width="100%">
</p>

# ClipTray LT

A lightweight clipboard manager for Linux (X11 & Wayland).

## Features

- Instant search and filtering as you type
- Full keyboard navigation (`Up`/`Down` arrows, `Enter` to paste, `Esc` to clear/close, `1`..`9` quick paste)
- Pin favorite items to keep them permanently
- Click or press Enter to paste directly into the active application
- Privacy protection: auto-ignores password managers (KeePass, Bitwarden, 1Password, etc.)
- Volatile RAM storage by default (optional disk persistence)
- Global `Super+V` hotkey

## Install

```bash
git clone https://github.com/rabbanyhmm/ClipTrayLT.git
cd ClipTrayLT
./install.sh
```

## Update

```bash
./update.sh
```

To uninstall: `./uninstall.sh`

## Commands

### Settings
```bash
# View configuration
cliptraylt config show

# Set item limit (default: 25)
cliptraylt config set max_items 50

# Save to disk (default: false / RAM only)
cliptraylt config set save_to_disk true

# Auto-ignore password managers (default: true)
cliptraylt config set ignore_password_managers true

# Reset to defaults
cliptraylt config reset
```

### Controls
```bash
cliptraylt --toggle    # Toggle flyout
cliptraylt --clear     # Clear unpinned items
cliptraylt --stop      # Stop daemon
cliptraylt --help      # Show help
```

## Keyboard Shortcuts

- `Super+V`: Open / close flyout
- `Up` / `Down`: Navigate clipboard history
- `Enter`: Paste selected item
- `1` - `9` (or `Alt+1` - `Alt+9`): Quick paste item
- `Esc`: Clear search / close flyout
