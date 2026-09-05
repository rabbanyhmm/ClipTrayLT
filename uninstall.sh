#!/usr/bin/env bash
set -e

echo "==> Uninstalling ClipTray LT..."

# Stop running daemon
pkill -f cliptraylt 2>/dev/null || true
systemctl --user stop cliptraylt.service 2>/dev/null || true
systemctl --user disable cliptraylt.service 2>/dev/null || true

# Remove binaries and symlinks
sudo rm -f /usr/local/bin/cliptraylt /usr/local/bin/cliptraylt-trigger /usr/local/bin/simpleclipboard-native /usr/local/bin/simpleclipboard-trigger

# Remove udev rules
sudo rm -f /etc/udev/rules.d/99-cliptraylt.rules
sudo udevadm control --reload-rules 2>/dev/null || true

# Remove service and autostart
TARGET_USER="${SUDO_USER:-$USER}"
TARGET_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
if [ -z "$TARGET_HOME" ]; then
    TARGET_HOME="$HOME"
fi

rm -f "$TARGET_HOME/.config/systemd/user/cliptraylt.service"
rm -f "$TARGET_HOME/.config/autostart/cliptraylt.desktop"

echo "==> ClipTray LT uninstalled successfully."
