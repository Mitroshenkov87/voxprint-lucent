#!/bin/sh
# Release .deb: C++ viewer, Qt 6.12 runtime, and the PySide6 tools package.
# AVIF and JPEG XL plugins are in the Windows installer. This package uses Qt's
# JPEG and WebP plugins, plus whatever image plugins the machine already has.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VERSION=$(sed -n 's/^version = "\(.*\)"/\1/p' "$ROOT/pyproject.toml" | head -n 1)
DEB_VERSION=$(printf '%s' "$VERSION" | sed 's/rc/~rc/; s/\([0-9]\)a/\1~a/; s/\([0-9]\)b/\1~b/')
TAG=${LUCENT_RELEASE_TAG:-v0.1.0-rc7}

QT_VERSION=6.12.0
QT_ROOT=${RUNNER_TEMP:-/tmp}/lucent-qt
QT_PREFIX="$QT_ROOT/$QT_VERSION/gcc_64"
if [ ! -e "$QT_PREFIX/lib/libQt6Core.so" ] && [ ! -e "$QT_PREFIX/lib/libQt6Core.so.6" ]; then
    python -m pip install --upgrade aqtinstall
    python -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir "$QT_ROOT" --archives qtbase icu
fi
if [ ! -e "$QT_PREFIX/plugins/imageformats/libqwebp.so" ]; then
    python -m pip install --upgrade aqtinstall
    python -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir "$QT_ROOT" --noarchives -m qtimageformats
fi
if [ ! -d "$QT_PREFIX" ]; then
    QT_PREFIX=$(find "$QT_ROOT/$QT_VERSION" -maxdepth 2 -type d -name gcc_64 | head -n 1)
fi
if [ -z "$QT_PREFIX" ]; then
    echo "Qt prefix not found under $QT_ROOT" >&2
    exit 1
fi

BUILD="$ROOT/cpp/build-release"
cmake -S "$ROOT/cpp" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD"
if [ ! -x "$BUILD/voxprint-lucent" ]; then
    echo "viewer binary missing" >&2
    exit 1
fi

STAGE=$(mktemp -d)
LIB="$STAGE/opt/voxprint-ai-suite/lucent"
mkdir -p "$LIB/lib" "$LIB/platforms" "$LIB/imageformats" "$LIB/share" \
    "$STAGE/usr/local/bin" "$STAGE/usr/share/applications" "$STAGE/DEBIAN"
cp "$BUILD/voxprint-lucent" "$LIB/voxprint-lucent"
cp "$ROOT/packaging/linux/qt.conf" "$LIB/qt.conf"
cp -a "$QT_PREFIX/lib/"*.so* "$LIB/lib/"
cp -a "$QT_PREFIX/plugins/platforms/"*.so "$LIB/platforms/"
if [ -d "$QT_PREFIX/plugins/imageformats" ]; then
    cp -a "$QT_PREFIX/plugins/imageformats/"*.so "$LIB/imageformats/" || true
fi
for plugin in libqjpeg.so libqwebp.so; do
    if [ ! -e "$LIB/imageformats/$plugin" ]; then
        echo "release package is missing $plugin" >&2
        exit 1
    fi
done
cp -a "$ROOT/share/." "$LIB/share/"
cp -a "$ROOT/voxprint_lucent" "$LIB/voxprint_lucent"
find "$LIB" -type d -name __pycache__ -exec rm -rf {} + || true
find "$LIB/lib" -name '*.a' -delete || true

cat > "$STAGE/usr/local/bin/voxprint-lucent" << 'EOF'
#!/bin/sh
ROOT=/opt/voxprint-ai-suite/lucent
export LD_LIBRARY_PATH="$ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$ROOT"
exec "$ROOT/voxprint-lucent" "$@"
EOF
cp "$STAGE/usr/local/bin/voxprint-lucent" "$STAGE/usr/local/bin/voxprint-lucent-cli"
cat > "$STAGE/usr/local/bin/voxprint-lucent-tools" << 'EOF'
#!/bin/sh
ROOT=/opt/voxprint-ai-suite/lucent
export PYTHONPATH="$ROOT${PYTHONPATH:+:$PYTHONPATH}"
exec python3 -m voxprint_lucent.tools_host "$@"
EOF
chmod 755 "$STAGE/usr/local/bin/voxprint-lucent" "$STAGE/usr/local/bin/voxprint-lucent-cli" \
    "$STAGE/usr/local/bin/voxprint-lucent-tools" "$LIB/voxprint-lucent"
cp "$ROOT/packaging/linux/ai.voxprint.Lucent.desktop" \
    "$STAGE/usr/share/applications/ai.voxprint.Lucent.desktop"
cat > "$STAGE/DEBIAN/control" << EOF
Package: voxprint-lucent
Version: ${DEB_VERSION}
Section: graphics
Priority: optional
Architecture: amd64
Depends: libc6, libegl1, libgl1, libglib2.0-0, libxkbcommon0, libfontconfig1, libdbus-1-3, libx11-6
Recommends: python3 (>= 3.14)
Maintainer: Aleksandr Mitroshenkov <223849179+Mitroshenkov87@users.noreply.github.com>
Description: VOXPrint AI Lucent image viewer
 Fast, colour-accurate image viewer for the Voxprint AI Media Suite.
 The viewer does not require an NVIDIA GPU. Tools and the Enlarge client
 need Python 3.14. AVIF and JPEG XL plugins ship in the Windows installer.
Homepage: https://github.com/Mitroshenkov87/voxprint-lucent
EOF
printf 'tag=%s\nqt=%s\npython-package=%s\navif-jxl=windows-installer\n' \
    "$TAG" "$QT_VERSION" "$VERSION" > "$LIB/VERSIONS.txt"

OUT="$ROOT/dist"
mkdir -p "$OUT"
dpkg-deb --root-owner-group --build "$STAGE" "$OUT/voxprint-lucent_${DEB_VERSION}_amd64.deb"
rm -rf "$STAGE"
echo "Wrote $OUT/voxprint-lucent_${DEB_VERSION}_amd64.deb"
