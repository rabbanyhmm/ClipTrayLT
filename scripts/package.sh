#!/usr/bin/env bash
set -euo pipefail

ARCH="${1:-x86_64}"
DEB_ARCH="${2:-amd64}"
VERSION="${3:-v1.0.0}"
VER_NUM="${VERSION#v}"
PKG_NAME="cliptraylt-${VERSION}-linux-${ARCH}"

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

mkdir -p "dist/${PKG_NAME}/bin"
cp build/cliptraylt "dist/${PKG_NAME}/bin/"
cp build/cliptraylt-trigger "dist/${PKG_NAME}/bin/"
cp install.sh "dist/${PKG_NAME}/"
cp update.sh "dist/${PKG_NAME}/"
cp uninstall.sh "dist/${PKG_NAME}/"
cp setup_shortcut.sh "dist/${PKG_NAME}/"
cp README.md "dist/${PKG_NAME}/"
cp LICENSE "dist/${PKG_NAME}/"
mkdir -p "dist/${PKG_NAME}/assets"
cp assets/banner.png "dist/${PKG_NAME}/assets/" 2>/dev/null || true
cp assets/icon.png "dist/${PKG_NAME}/assets/" 2>/dev/null || true

# Build Debian .deb package
DEB_DIR="dist/deb_root_${DEB_ARCH}"
rm -rf "${DEB_DIR}"
mkdir -p "${DEB_DIR}/DEBIAN"
mkdir -p "${DEB_DIR}/usr/local/bin"
mkdir -p "${DEB_DIR}/usr/share/applications"
mkdir -p "${DEB_DIR}/usr/share/icons/hicolor/256x256/apps"
mkdir -p "${DEB_DIR}/usr/share/pixmaps"
mkdir -p "${DEB_DIR}/usr/share/doc/cliptraylt"
mkdir -p "${DEB_DIR}/etc/udev/rules.d"

cp build/cliptraylt "${DEB_DIR}/usr/local/bin/"
cp build/cliptraylt-trigger "${DEB_DIR}/usr/local/bin/"
cp assets/icon.png "${DEB_DIR}/usr/share/icons/hicolor/256x256/apps/cliptraylt.png"
cp assets/icon.png "${DEB_DIR}/usr/share/pixmaps/cliptraylt.png"
cp README.md "${DEB_DIR}/usr/share/doc/cliptraylt/README.md"
cp LICENSE "${DEB_DIR}/usr/share/doc/cliptraylt/copyright"

cat > "${DEB_DIR}/usr/share/applications/cliptraylt.desktop" << 'DESKTOPEOF'
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
DESKTOPEOF

printf 'KERNEL=="uinput", MODE="0666"\nSUBSYSTEM=="input", KERNEL=="event*", MODE="0666"\n' > "${DEB_DIR}/etc/udev/rules.d/99-cliptraylt.rules"

cat > "${DEB_DIR}/DEBIAN/control" << CONTROLEOF
Package: cliptraylt
Version: ${VER_NUM}
Section: utils
Priority: optional
Architecture: ${DEB_ARCH}
Depends: libc6, libqt6core6t64 | libqt6core6, libqt6gui6t64 | libqt6gui6, libqt6widgets6t64 | libqt6widgets6, libqt6network6t64 | libqt6network6, libqt6dbus6t64 | libqt6dbus6, libsqlite3-0, libevdev2, libx11-6, libxcb1
Maintainer: ClipTray LT Contributors <rabbany@users.noreply.github.com>
Homepage: https://github.com/rabbanyhmm/ClipTrayLT
Description: Lightweight Windows 10 style clipboard flyout manager for Linux
 ClipTray LT is an ultra-fast, lightweight clipboard manager for Linux
 featuring instant search, keyboard navigation, password manager auto-exclusion,
 and zero background CPU usage.
CONTROLEOF

cat > "${DEB_DIR}/DEBIAN/postinst" << 'POSTINSTEOF'
#!/bin/sh
set -e
if command -v udevadm >/dev/null 2>&1; then
    udevadm control --reload-rules 2>/dev/null || true
    udevadm trigger 2>/dev/null || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database -q 2>/dev/null || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi
chmod 666 /dev/uinput /dev/input/event* 2>/dev/null || true
POSTINSTEOF
chmod 755 "${DEB_DIR}/DEBIAN/postinst"

cat > "${DEB_DIR}/DEBIAN/prerm" << 'PRERMEOF'
#!/bin/sh
set -e
pkill -f cliptraylt 2>/dev/null || true
PRERMEOF
chmod 755 "${DEB_DIR}/DEBIAN/prerm"

dpkg-deb --build --root-owner-group "${DEB_DIR}" "dist/cliptraylt_${VER_NUM}_${DEB_ARCH}.deb"
rm -rf "${DEB_DIR}"

cd dist
tar -czvf "${PKG_NAME}.tar.gz" "${PKG_NAME}"
sha256sum "${PKG_NAME}.tar.gz" > "${PKG_NAME}.tar.gz.sha256"
sha256sum "cliptraylt_${VER_NUM}_${DEB_ARCH}.deb" > "cliptraylt_${VER_NUM}_${DEB_ARCH}.deb.sha256"
