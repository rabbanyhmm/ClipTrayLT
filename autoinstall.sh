#!/usr/bin/env bash
# Wrapper to download and install a project from a URL
set -e

if [ -z "$1" ]; then
  echo "Usage: $0 <repository_url>"
  exit 1
fi

REPO_URL="$1"
PROJECT_NAME="cliptraylt-auto-install"

echo "==> Cloning $REPO_URL..."
git clone "$REPO_URL" "$PROJECT_NAME"
cd "$PROJECT_NAME"

echo "==> Running installer..."
sudo ./install.sh
