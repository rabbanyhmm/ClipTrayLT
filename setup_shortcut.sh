#!/bin/bash
# Registers Super+V in GNOME desktop settings to trigger WinClipboard app

VENV_PYTHON="/home/rabbany/Desktop/ClipBoard/.venv/bin/python"
MAIN_PY="/home/rabbany/Desktop/ClipBoard/main.py"
TOGGLE_CMD="$VENV_PYTHON $MAIN_PY --toggle"

echo "Configuring Linux GNOME desktop shortcut Super+V -> $TOGGLE_CMD"

# GNOME custom keybinding path setup
KEYBIND_PATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom-win-clipboard/"

# Update GNOME custom keybindings list
EXISTING=$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
if [[ "$EXISTING" != *"$KEYBIND_PATH"* ]]; then
    if [[ "$EXISTING" == "@as []" ]] || [[ "$EXISTING" == "[]" ]]; then
        gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['$KEYBIND_PATH']"
    else
        NEW_LIST=$(echo "$EXISTING" | sed "s|\]|, '$KEYBIND_PATH']|")
        gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "$NEW_LIST"
    fi
fi

# Set custom keybinding properties
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEYBIND_PATH name 'WinClipboard Flyout'
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEYBIND_PATH command "$TOGGLE_CMD"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEYBIND_PATH binding '<Super>v'

echo "GNOME shortcut setup complete. Super+V will now trigger the Clipboard app natively!"
