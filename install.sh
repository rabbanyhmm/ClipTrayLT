#!/usr/bin/env bash
set -e

echo "==> Installing ClipTray LT..."

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# 1. Check for pre-built binaries or build from source
if [[ -f "$DIR/bin/cliptraylt" && -f "$DIR/bin/cliptraylt-trigger" ]]; then
    echo "==> Found pre-built binaries in bin/."
    BIN_DIR="$DIR/bin"
else
    if command -v apt-get >/dev/null 2>&1; then
        echo "==> Checking build dependencies..."
        sudo apt-get update -qq || true
        sudo apt-get install -y -qq build-essential cmake qt6-base-dev libevdev-dev libsqlite3-dev libatspi2.0-dev libglib2.0-dev || true
    fi

    echo "==> Building binaries from source..."
    cmake -B "$DIR/build" -S "$DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$DIR/build" -j"$(nproc)"
    BIN_DIR="$DIR/build"
fi

# 2. Stop old daemon if running
echo "==> Stopping any previous instances..."
"$BIN_DIR/cliptraylt" --stop 2>/dev/null || true
pkill -f cliptraylt 2>/dev/null || true
pkill -f simpleclipboard 2>/dev/null || true

# 3. Install binaries
echo "==> Installing to /usr/local/bin..."
sudo install -m 755 "$BIN_DIR/cliptraylt" /usr/local/bin/cliptraylt
sudo install -m 755 "$BIN_DIR/cliptraylt-trigger" /usr/local/bin/cliptraylt-trigger
sudo ln -sf /usr/local/bin/cliptraylt /usr/local/bin/simpleclipboard-native
sudo ln -sf /usr/local/bin/cliptraylt-trigger /usr/local/bin/simpleclipboard-trigger

# 4b. Install application icon
if [[ -f "$DIR/assets/icon.png" ]]; then
    sudo mkdir -p /usr/share/icons/hicolor/256x256/apps /usr/share/pixmaps 2>/dev/null || true
    sudo install -m 644 "$DIR/assets/icon.png" /usr/share/icons/hicolor/256x256/apps/cliptraylt.png 2>/dev/null || true
    sudo install -m 644 "$DIR/assets/icon.png" /usr/share/pixmaps/cliptraylt.png 2>/dev/null || true
fi

# 5. Persistent device permissions for keyboard paste injection and hotkeys
echo "==> Setting up hardware access permissions (udev)..."
printf 'KERNEL=="uinput", MODE="0666"\nSUBSYSTEM=="input", KERNEL=="event*", MODE="0666"\n' | sudo tee /etc/udev/rules.d/99-cliptraylt.rules >/dev/null
sudo udevadm control --reload-rules 2>/dev/null || true
sudo udevadm trigger 2>/dev/null || true
sudo chmod 666 /dev/uinput /dev/input/event* 2>/dev/null || true

# 6. Configure GNOME shortcut if available
if command -v gsettings >/dev/null 2>&1; then
    echo "==> Configuring Super+V shortcut..."
    "$DIR/setup_shortcut.sh" || true
fi

# 7. Autostart on login (Desktop autostart + systemd user service)
TARGET_USER="${SUDO_USER:-$USER}"
TARGET_HOME=$(getent passwd "$TARGET_USER" | cut -d: -f6)
if [[ -z "$TARGET_HOME" ]]; then
    TARGET_HOME="$HOME"
fi

mkdir -p "$TARGET_HOME/.config/autostart"
cat > "$TARGET_HOME/.config/autostart/cliptraylt.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=ClipTray LT
Comment=Lightweight Clipboard Manager
Exec=/usr/local/bin/cliptraylt
Icon=cliptraylt
Terminal=false
Categories=Utility;
StartupNotify=false
X-GNOME-Autostart-enabled=true
EOF
chown -R "$TARGET_USER:" "$TARGET_HOME/.config/autostart" 2>/dev/null || true

mkdir -p "$TARGET_HOME/.config/systemd/user"
cat > "$TARGET_HOME/.config/systemd/user/cliptraylt.service" << 'EOF'
[Unit]
Description=ClipTray LT Clipboard Manager
After=graphical-session.target

[Service]
ExecStart=/usr/local/bin/cliptraylt
Restart=always
RestartSec=2
Environment=QT_QPA_PLATFORM=xcb

[Install]
WantedBy=default.target
EOF
chown -R "$TARGET_USER:" "$TARGET_HOME/.config/systemd" 2>/dev/null || true

if [[ -n "$SUDO_USER" ]]; then
    sudo -u "$SUDO_USER" systemctl --user daemon-reload 2>/dev/null || true
    sudo -u "$SUDO_USER" systemctl --user enable --now cliptraylt.service 2>/dev/null || true
else
    systemctl --user daemon-reload 2>/dev/null || true
    systemctl --user enable --now cliptraylt.service 2>/dev/null || true
fi

# If systemctl user service isn't active, launch in background
if ! pgrep -f "/usr/local/bin/cliptraylt" >/dev/null 2>&1; then
    /usr/local/bin/cliptraylt >/dev/null 2>&1 &
fi

echo ""
echo "========================================"
echo " ClipTray LT installed successfully!"
echo "========================================"
echo " Shortcut : Press Super+V to open flyout"
echo " Settings : cliptraylt config show"
echo " Help     : cliptraylt --help"
echo ""
