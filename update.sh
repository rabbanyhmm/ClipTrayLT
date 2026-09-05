#!/usr/bin/env bash
set -e

echo "==> Updating ClipTray LT..."
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Pull latest commits
git pull origin main

# Run installer to rebuild and reload daemon
./install.sh

echo "==> Update complete!"
