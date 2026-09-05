#!/bin/bash
# Script to register Super+V global shortcut in GNOME Desktop settings
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRIGGER="$DIR/build/cliptraylt-trigger"

if [ ! -f "$TRIGGER" ]; then
    echo "Building trigger CLI..."
    cmake -B "$DIR/build" -S "$DIR"
    cmake --build "$DIR/build" --target cliptraylt-trigger
fi

if [ -f "/usr/local/bin/cliptraylt-trigger" ]; then
    COMMAND="/usr/local/bin/cliptraylt-trigger"
elif [ -f "/usr/local/bin/cliptraylt" ]; then
    COMMAND="/usr/local/bin/cliptraylt --toggle"
else
    COMMAND="$TRIGGER"
fi

NAME="Windows 10 Clipboard Flyout"
BINDING="<Super>v"

echo "Configuring GNOME Custom Shortcut for Super+V..."

CURRENT_LIST=$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)
NEW_PATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/simpleclipboard-native/"

if [[ "$CURRENT_LIST" != *"$NEW_PATH"* ]]; then
    if [ "$CURRENT_LIST" == "@as []" ] || [ -z "$CURRENT_LIST" ]; then
        UPDATED_LIST="['$NEW_PATH']"
    else
        UPDATED_LIST="${CURRENT_LIST%]*}, '$NEW_PATH']"
    fi
    gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "$UPDATED_LIST"
fi

gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$NEW_PATH name "$NAME"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$NEW_PATH command "$COMMAND"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$NEW_PATH binding "$BINDING"

echo "✓ Registered Super+V shortcut -> '$COMMAND'"
