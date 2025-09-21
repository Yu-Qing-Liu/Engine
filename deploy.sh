#!/usr/bin/env bash
# Minimal AppImage packer that mirrors native behavior:
# - No bundling of graphics libs (Vulkan, X11, Wayland, GLFW, etc.)
# - No RPATH/RUNPATH edits, no VK_* env forcing.
# - Places a .desktop file in usr/share/applications and a root-level symlink.
# - Works around mksquashfs reproducible-build conflict by unsetting SOURCE_DATE_EPOCH.

set -euo pipefail

APP_NAME="${APP_NAME:-Engine}"                   # binary name to run
APP_BIN_REL="${APP_BIN_REL:-bin/Engine}"         # path to your built binary
ASSETS_DIR_REL="${ASSETS_DIR_REL:-bin/assets}"   # optional assets dir
ICON_PATH="${ICON_PATH:-}"                       # optional 256x256 PNG
OUT_NAME="${OUT_NAME:-${APP_NAME}-x86_64.AppImage}"

APPIMAGETOOL="${APPIMAGETOOL:-}"                 # optional path override

msg()  { printf "\033[1;32m==>\033[0m %s\n" "$*"; }
warn() { printf "\033[1;33mWARN:\033[0m %s\n" "$*\n"; }
err()  { printf "\033[1;31mERROR:\033[0m %s\n" "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# Make AppImage runnable on Nix without FUSE
: "${APPIMAGE_EXTRACT_AND_RUN:=1}"
export APPIMAGE_EXTRACT_AND_RUN

# Sanity
[[ -f "$APP_BIN_REL" ]] || err "Binary not found: $APP_BIN_REL"
[[ -x "$APP_BIN_REL" ]] || err "Binary not executable: $APP_BIN_REL (chmod +x)"

# Prepare AppDir
APPDIR="AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/16x16/apps" \
         "$APPDIR/usr/share/icons/hicolor/32x32/apps" \
         "$APPDIR/usr/share/icons/hicolor/64x64/apps" \
         "$APPDIR/usr/share/icons/hicolor/128x128/apps" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$APPDIR/usr/share/icons/hicolor/scalable/apps"

DESKTOP_NAME="$(echo "$APP_NAME" | tr '[:upper:]' '[:lower:]')"
DESKTOP_PATH="$APPDIR/usr/share/applications/${DESKTOP_NAME}.desktop"

# Desktop file (simple, valid)
cat > "$DESKTOP_PATH" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Exec=${APP_NAME}
Icon=${DESKTOP_NAME}
Categories=Graphics;
Terminal=false
EOF

# Symlink desktop file into AppDir root for picky appimagetool builds
ln -sf "usr/share/applications/${DESKTOP_NAME}.desktop" "$APPDIR/${DESKTOP_NAME}.desktop"

# Icon
ICON_HICOLOR="$APPDIR/usr/share/icons/hicolor/256x256/apps/${DESKTOP_NAME}.png"
ICON_TOP="$APPDIR/${DESKTOP_NAME}.png"
if [[ -n "$ICON_PATH" && -f "$ICON_PATH" ]]; then
  cp "$ICON_PATH" "$ICON_HICOLOR"
  msg "Icon copied -> $ICON_HICOLOR"
else
  msg "No ICON_PATH; embedding minimal placeholder icon."
  cat > "$ICON_TOP".b64 <<'B64'
iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMB/UrwE+UAAAAASUVORK5CYII=
B64
  base64 -d "$ICON_TOP".b64 > "$ICON_TOP"
  rm -f "$ICON_TOP".b64
  cp "$ICON_TOP" "$ICON_HICOLOR"
fi
# Some launchers look for AppDir/<name>.png
if [[ -f "$ICON_HICOLOR" && ! -f "$ICON_TOP" ]]; then
  cp "$ICON_HICOLOR" "$ICON_TOP"
fi

# Copy binary + assets (no libraries)
cp "$APP_BIN_REL" "$APPDIR/usr/bin/${APP_NAME}"
chmod +x "$APPDIR/usr/bin/${APP_NAME}"
if [[ -d "$ASSETS_DIR_REL" ]]; then
  msg "Found assets at: $ASSETS_DIR_REL"
  cp -r "$ASSETS_DIR_REL" "$APPDIR/usr/bin/assets"
fi

# AppRun — just exec the binary; no env forcing
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
cd "$HERE/usr/bin"
# Mirror native behavior: do not force Vulkan/X11/Wayland/layers.
if [ -x "./Engine" ]; then exec ./Engine "$@"; fi
exec ./* "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Quick verification
[[ -f "$DESKTOP_PATH" ]] || err "Desktop file missing at $DESKTOP_PATH"
[[ -f "$APPDIR/${DESKTOP_NAME}.desktop" ]] || err "Root-level .desktop symlink missing"

# Resolve appimagetool
have curl || err "'curl' is required to fetch appimagetool."
APPIMAGETOOL_BIN=""
if [[ -n "${APPIMAGETOOL}" ]]; then
  APPIMAGETOOL_BIN="$APPIMAGETOOL"
elif have appimagetool; then
  APPIMAGETOOL_BIN="$(command -v appimagetool)"
elif have appimagetool-x86_64.AppImage; then
  APPIMAGETOOL_BIN="$(command -v appimagetool-x86_64.AppImage)"
else
  mkdir -p tools
  APPIMAGETOOL_BIN="tools/appimagetool-x86_64.AppImage"
  if [[ ! -x "$APPIMAGETOOL_BIN" ]]; then
    msg "Downloading appimagetool …"
    curl -fsSL -o "$APPIMAGETOOL_BIN" \
      https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x "$APPIMAGETOOL_BIN"
  fi
fi

msg "Building AppImage…"
rm -f ./*.AppImage || true

# Work around: mksquashfs reproducible-build conflict.
# Some environments export SOURCE_DATE_EPOCH, while appimagetool passes timestamp flags.
# Unset SOURCE_DATE_EPOCH *just for this invocation*.
if ! env -u SOURCE_DATE_EPOCH "$APPIMAGETOOL_BIN" "$APPDIR" "${OUT_NAME}.tmp" >/dev/null 2>&1; then
  env -u SOURCE_DATE_EPOCH "$APPIMAGETOOL_BIN" "$APPDIR" "${OUT_NAME}.tmp"
fi

[[ -f "${OUT_NAME}.tmp" ]] || err "No AppImage produced (appimagetool failed)."
mv -f "${OUT_NAME}.tmp" "${OUT_NAME}"
chmod +x "${OUT_NAME}"
msg "Done: ./${OUT_NAME}"

cat <<'EON'
----------------------------------------------------------------
Run it (mirrors native behavior):
  ./Engine-x86_64.AppImage

Notes:
  • No graphics libs are bundled; your app will auto-pick ICDs/layers/displays.
  • If you ever need a portable build for non-Nix hosts, we can add a
    separate “portable mode” script without touching this minimal path.
----------------------------------------------------------------
EON

