#!/usr/bin/env bash
# Build the Linux deliverables from a built Qt app:
#   StickySnap-linux-x86_64.AppImage   single-file app, no install
#   StickySnap-linux-x86_64.deb        menu entry + /opt install
#   StickySnap-linux-x86_64.tar.gz     portable directory
#
# Usage: packaging/linux/package.sh <build-dir> <output-dir> <version>
#
# Requires: linuxdeploy + linuxdeploy-plugin-qt (downloaded below), qmake in
# PATH, dpkg-deb, curl. Linux X11 only (Wayland is not supported).
set -euo pipefail

BUILD_DIR="${1:?usage: package.sh <build-dir> <out-dir> <version>}"
OUT_DIR="${2:?}"
VERSION="${3:?}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$(cd "$BUILD_DIR" && pwd)"
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
APP="$BUILD_DIR/StickySnap"
[ -x "$APP" ] || { echo "missing executable: $APP" >&2; exit 1; }

QMAKE="$(command -v qmake6 || command -v qmake || true)"
[ -n "$QMAKE" ] || { echo "qmake not found in PATH" >&2; exit 1; }
export QMAKE

WORK="$(mktemp -d "${TMPDIR:-/tmp}/stickysnap-linux.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
TOOLS="$WORK/tools"
mkdir -p "$TOOLS"
echo "==> downloading linuxdeploy"
curl -fsSL --retry 3 -o "$TOOLS/linuxdeploy" \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl -fsSL --retry 3 -o "$TOOLS/linuxdeploy-plugin-qt" \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$TOOLS/linuxdeploy" "$TOOLS/linuxdeploy-plugin-qt"
# linuxdeploy discovers plugins on PATH.
export PATH="$TOOLS:$PATH"

# GitHub runners do not have FUSE; extract-and-run makes the AppImage tools work.
export APPIMAGE_EXTRACT_AND_RUN=1
APPDIR="$WORK/AppDir"
mkdir -p "$APPDIR"
echo "==> building AppDir"
(
  cd "$WORK"
  "$TOOLS/linuxdeploy" \
    --appdir "$APPDIR" \
    --executable "$APP" \
    --desktop-file "$ROOT/packaging/linux/stickysnap.desktop" \
    --icon-file "$ROOT/portable/resources/app-icon.png" \
    --icon-filename stickysnap \
    --plugin qt \
    --output appimage
)

APPIMAGE="$(find "$WORK" -maxdepth 1 -name 'StickySnap*.AppImage' -print -quit)"
[ -n "$APPIMAGE" ] || { echo "linuxdeploy did not produce an AppImage" >&2; exit 1; }
cp "$APPIMAGE" "$OUT_DIR/StickySnap-linux-x86_64.AppImage"

echo "==> building portable tar.gz"
cp -a "$APPDIR" "$WORK/StickySnap-linux"
cat > "$WORK/StickySnap-linux/run.sh" <<'EOS'
#!/bin/sh
cd "$(dirname "$0")"
exec ./AppRun "$@"
EOS
chmod +x "$WORK/StickySnap-linux/run.sh"
tar -czf "$OUT_DIR/StickySnap-linux-x86_64.tar.gz" -C "$WORK" StickySnap-linux

echo "==> building .deb"
DEB="$WORK/deb"
mkdir -p \
  "$DEB/DEBIAN" \
  "$DEB/opt/stickysnap" \
  "$DEB/usr/bin" \
  "$DEB/usr/share/applications" \
  "$DEB/usr/share/icons/hicolor/256x256/apps"
cp -a "$APPDIR/usr/." "$DEB/opt/stickysnap/"
cp "$ROOT/packaging/linux/stickysnap.desktop" \
  "$DEB/usr/share/applications/stickysnap.desktop"
cp "$ROOT/portable/resources/app-icon.png" \
  "$DEB/usr/share/icons/hicolor/256x256/apps/stickysnap.png"
cat > "$DEB/usr/bin/StickySnap" <<'EOS'
#!/bin/sh
export LD_LIBRARY_PATH="/opt/stickysnap/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="/opt/stickysnap/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
exec /opt/stickysnap/bin/StickySnap "$@"
EOS
chmod 755 "$DEB/usr/bin/StickySnap"
ln -s StickySnap "$DEB/usr/bin/stickysnap"
cat > "$DEB/DEBIAN/control" <<EOF
Package: stickysnap
Version: $VERSION
Section: graphics
Priority: optional
Architecture: amd64
Maintainer: CarlOpenLab <noreply@carlopenlab.github.io>
Homepage: https://carlopenlab.github.io/StickySnap/
Depends: libc6, libx11-6, libxext6, libxkbcommon0, libxkbcommon-x11-0, libgl1, libegl1, libfontconfig1, libdbus-1-3, libxcb1, libxcb-icccm4, libxcb-keysyms1, libxcb-render-util0, libxcb-shape0, libxcb-xinerama0, libxcb-xkb1, libxcb-cursor0
Description: Pin-first screenshot and annotation tool
 StickySnap captures a region or window, annotates it in place, and pins
 the result to the desktop. This package bundles the Qt runtime and runs
 in X11 sessions; Wayland screen capture is not supported yet.
EOF
dpkg-deb --build --root-owner-group "$DEB" \
  "$OUT_DIR/StickySnap-linux-x86_64.deb" >/dev/null

echo "==> checksums"
(cd "$OUT_DIR" && sha256sum \
  StickySnap-linux-x86_64.AppImage \
  StickySnap-linux-x86_64.deb \
  StickySnap-linux-x86_64.tar.gz > StickySnap-linux-x86_64-sha256.txt)

echo "==> assets:"
ls -lh "$OUT_DIR"/StickySnap-linux-*
