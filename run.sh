#!/bin/bash
# Configure local device permissions so hardware keyboard input injection works without sudo

echo "=== Clipboard Flyout Setup ==="

# 1. Grant write permissions to uinput device for hardware keyboard emulation
if [ ! -w /dev/uinput ]; then
    echo "[1/2] Granting read/write access to /dev/uinput..."
    sudo chmod 666 /dev/uinput
else
    echo "[1/2] /dev/uinput permissions are already configured."
fi

# 2. Grant read permissions to hardware keyboard input event streams
echo "[2/2] Granting read access to hardware keyboard input streams..."
sudo chmod 666 /dev/input/event*

# Build binary if not already built
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/build/cliptraylt"

if [ ! -f "$BIN" ]; then
    echo "Building ClipTray LT Native..."
    cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$SCRIPT_DIR/build" -j$(nproc)
fi

echo "Starting ClipTray LT Native..."
exec sudo env WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}" DISPLAY="${DISPLAY:-:0}" XDG_RUNTIME_DIR="/run/user/$(id -u)" DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus" SUDO_USER="$USER" "$BIN"
