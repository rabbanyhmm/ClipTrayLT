#!/bin/bash
set -e

# Change directory to the script's directory
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$DIR/build/simpleclipboard-native"

# 1. Build if not already built
if [ ! -f "$BIN" ]; then
    echo "[SimpleClipboard] Building native C++ application..."
    cmake -B "$DIR/build" -S "$DIR"
    cmake --build "$DIR/build"
fi

# 2. Configure uinput permissions
if [ ! -w /dev/uinput ]; then
    echo "[SimpleClipboard] Configuring /dev/uinput permissions..."
    sudo chmod 666 /dev/uinput 2>/dev/null || true
fi

# 3. Check if user wants to run as root
if [ "$1" == "--root" ] || [ "$EUID" -ne 0 ]; then
    echo "[SimpleClipboard] Starting with root hardware access..."
    # Preserve current user session display environment
    USER_UID=$(id -u)
    sudo env \
        WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}" \
        DISPLAY="${DISPLAY:-:0}" \
        XDG_RUNTIME_DIR="/run/user/${USER_UID}" \
        XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" \
        SUDO_USER="$USER" \
        "$BIN" "${@:2}"
else
    # Already running as root or direct execution
    exec "$BIN" "$@"
fi
