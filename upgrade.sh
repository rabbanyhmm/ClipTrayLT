#!/usr/bin/env bash
# Wrapper to update/upgrade the installation locally
set -e

# Ensure script is run as root
if [[ $EUID -ne 0 ]]; then
   echo "==> This script must be run as root."
   echo "==> Please run: sudo ./upgrade.sh"
   exit 1
fi

# Determine project directory (assuming it was cloned to /opt/cliptraylt or similar)
# Since we don't have a fixed location, we look for it in common places
PROJECT_DIR="/opt/cliptraylt"

if [ ! -d "$PROJECT_DIR" ]; then
    echo "==> Could not find project directory at $PROJECT_DIR."
    echo "==> Please run the installation script from inside the project directory."
    exit 1
fi

echo "==> Upgrading ClipTray LT from $PROJECT_DIR..."
cd "$PROJECT_DIR"
git pull
sudo ./install.sh

echo "==> Upgrade complete."
