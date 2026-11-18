#!/usr/bin/env bash
set -e

# Ensure script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "==> This script must be run as root."
   echo "==> Please run: sudo ./install.sh"
   exit 1
fi

echo "==> Installing ClipTray LT..."

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    OS=unknown
fi

echo "==> Detecting OS: $OS"

# Function to check and install dependencies cleanly
install_deps() {
    local pkgs=("$@")
    for pkg in "${pkgs[@]}"; do
        if ! command -v "$pkg" >/dev/null 2>&1 && ! dpkg -s "$pkg" >/dev/null 2>&1 && ! rpm -q "$pkg" >/dev/null 2>&1 && ! pacman -Qi "$pkg" >/dev/null 2>&1; then
            echo "==> Installing dependency: $pkg"
            case $OS in
                ubuntu|debian|pop|mint) apt-get install -y -qq "$pkg" ;;
                fedora|rhel|centos|almalinux) dnf install -y -q "$pkg" ;;
                arch|manjaro|endeavouros) pacman -S --noconfirm --needed "$pkg" ;;
            esac
        else
            echo "==> Dependency already installed: $pkg"
        fi
    done
}

case $OS in
    ubuntu|debian|pop|mint)
        max_retries=3
        count=0
        until apt-get update -qq || [ $count -ge $max_retries ]; do
            echo "==> apt-get update failed, retrying ($((count+1))...)"
            sleep 5
            count=$((count+1))
        done
        install_deps build-essential cmake qt6-base-dev libevdev-dev libsqlite3-dev libatspi2-dev libglib2.0-dev git
        ;;
    fedora|rhel|centos|almalinux)
        install_deps cmake gcc-c++ qt6-qtbase-devel libevdev-devel sqlite-devel at-spi2-core-devel glib2-devel git
        ;;
    arch|manjaro|endeavouros)
        install_deps base-devel cmake qt6-base libevdev sqlite at-spi2-core glib2 git
        ;;
    *)
        echo "==> Automated dependency installation not supported for: $OS."
        echo "==> Please install: cmake, C++ compiler, qt6, libevdev, sqlite3, at-spi2, glib2, git manually."
        ;;
esac

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Build from source
echo "==> Building binaries for current architecture: $(uname -m)..."
rm -rf "$DIR/build"
cmake -B "$DIR/build" -S "$DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$DIR/build" -j"$(nproc)"
BIN_DIR="$DIR/build"

# Stop old daemon if running
echo "==> Stopping any previous instances..."
if command -v cliptraylt >/dev/null 2>&1; then
    cliptraylt --stop 2>/dev/null || true
fi
pkill -f cliptraylt-bin 2>/dev/null || true
pkill -f simpleclipboard 2>/dev/null || true

# Install actual binaries
echo "==> Installing binaries..."
install -m 755 "$BIN_DIR/cliptraylt" /usr/local/bin/cliptraylt-bin
install -m 755 "$BIN_DIR/cliptraylt-trigger" /usr/local/bin/cliptraylt-trigger

# Install wrapper script
echo "==> Installing CLI wrapper..."
cat > /usr/local/bin/cliptraylt << EOF
#!/usr/bin/env bash
VERSION=\$(cat "$DIR/VERSION")
case "\$1" in
    upgrade)
        echo "==> Upgrading ClipTray LT..."
        cd "$DIR" && git pull && sudo ./install.sh
        ;;
    --version)
        echo "ClipTray LT version \$VERSION"
        ;;
    *)
        /usr/local/bin/cliptraylt-bin "\$@"
        ;;
esac
EOF
chmod +x /usr/local/bin/cliptraylt

# Install application icon
if [[ -f "$DIR/assets/icon.png" ]]; then
    mkdir -p /usr/share/icons/hicolor/256x256/apps /usr/share/pixmaps 2>/dev/null || true
    install -m 644 "$DIR/assets/icon.png" /usr/share/icons/hicolor/256x256/apps/cliptraylt.png 2>/dev/null || true
    install -m 644 "$DIR/assets/icon.png" /usr/share/pixmaps/cliptraylt.png 2>/dev/null || true
fi

# Persistent device permissions
echo "==> Setting up hardware access permissions (udev)..."
printf 'KERNEL=="uinput", MODE="0666"\nSUBSYSTEM=="input", KERNEL=="event*", MODE="0666"\n' | tee /etc/udev/rules.d/99-cliptraylt.rules >/dev/null
udevadm control --reload-rules 2>/dev/null || true
udevadm trigger 2>/dev/null || true
chmod 666 /dev/uinput /dev/input/event* 2>/dev/null || true

# Configure GNOME shortcut if available
if command -v gsettings >/dev/null 2>&1; then
    echo "==> Configuring Super+V shortcut..."
    "$DIR/setup_shortcut.sh" || true
fi

# Autostart on login
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
Exec=/usr/local/bin/cliptraylt-bin
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
ExecStart=/usr/local/bin/cliptraylt-bin
Restart=always
RestartSec=2
Environment=QT_QPA_PLATFORM=xcb

[Install]
WantedBy=default.target
EOF
chown -R "$TARGET_USER:" "$TARGET_HOME/.config/systemd" 2>/dev/null || true

# Apply settings for the user
sudo -u "$TARGET_USER" systemctl --user daemon-reload 2>/dev/null || true
sudo -u "$TARGET_USER" systemctl --user enable --now cliptraylt.service 2>/dev/null || true

# If systemctl user service isn't active, launch in background
if ! pgrep -f "/usr/local/bin/cliptraylt-bin" >/dev/null 2>&1; then
    sudo -u "$TARGET_USER" /usr/local/bin/cliptraylt-bin >/dev/null 2>&1 &
fi

echo ""
echo "========================================"
echo " ClipTray LT installed successfully!"
echo "========================================"
echo " Shortcut : Press Super+V to open flyout"
echo " Usage    : cliptraylt upgrade"
echo " Version  : cliptraylt --version"
echo " Settings : cliptraylt config show"
echo " Help     : cliptraylt --help"
echo ""
