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

# 3. Execute application under the standard user context
echo "Starting Windows Clipboard Flyout under standard user session..."
.venv/bin/python main.py
