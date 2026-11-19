#!/bin/sh
# Linux .deb: the C++ viewer with native AVIF and JPEG XL (libavif, libjxl),
# the Qt 6.12 runtime, and an embedded Python 3.14 runtime for the PySide6
# tools helper. Everything lands in /opt/voxprint-ai-suite/lucent.
# Needs: cmake, ninja, a C++ compiler, uv, libavif-dev, libjxl-dev, dpkg-deb.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
VERSION=$(sed -n 's/^version = "\(.*\)"/\1/p' "$ROOT/pyproject.toml" | head -n 1)
DEB_VERSION=$(printf '%s' "$VERSION" | sed 's/rc/~rc/; s/\([0-9]\)a/\1~a/; s/\([0-9]\)b/\1~b/')
FILE_VERSION=$VERSION
BUILD_NUMBER=$(sed -n 's/.*"number" *: *\([0-9][0-9]*\).*/\1/p' "$ROOT/BUILD.json")
PYTHON_VERSION=${LUCENT_PYTHON_VERSION:-3.14.10}
COMMON_REQ=$(sed -n 's/^ *"voxprint-common\[ui\] @ \(git+[^"]*\)".*/voxprint-common @ \1/p' "$ROOT/pyproject.toml")
if [ -z "$COMMON_REQ" ]; then
    echo "voxprint-common git pin not found in pyproject.toml" >&2
    exit 1
fi

QT_VERSION=6.12.0
QT_ROOT=${RUNNER_TEMP:-/tmp}/lucent-qt
QT_PREFIX="$QT_ROOT/$QT_VERSION/gcc_64"
if [ ! -e "$QT_PREFIX/lib/libQt6Core.so" ] && [ ! -e "$QT_PREFIX/lib/libQt6Core.so.6" ]; then
    python3 -m pip install --upgrade aqtinstall
    python3 -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir "$QT_ROOT" --archives qtbase icu
fi
if [ ! -e "$QT_PREFIX/plugins/imageformats/libqwebp.so" ]; then
    python3 -m pip install --upgrade aqtinstall
    python3 -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 \
        --outputdir "$QT_ROOT" --noarchives -m qtimageformats
fi
if [ ! -d "$QT_PREFIX" ]; then
    QT_PREFIX=$(find "$QT_ROOT/$QT_VERSION" -maxdepth 2 -type d -name gcc_64 | head -n 1)
fi
if [ -z "$QT_PREFIX" ]; then
    echo "Qt prefix not found under $QT_ROOT" >&2
    exit 1
fi

for module in libavif libjxl libjxl_threads; do
    if ! pkg-config --exists "$module"; then
        echo "$module development files are missing (apt-get install libavif-dev libjxl-dev)" >&2
        exit 1
    fi
done

BUILD="$ROOT/cpp/build-release"
cmake -S "$ROOT/cpp" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD"
if [ ! -x "$BUILD/voxprint-lucent" ]; then
    echo "viewer binary missing" >&2
    exit 1
fi
for lib in libavif libjxl; do
    if ! LD_LIBRARY_PATH="$QT_PREFIX/lib" ldd "$BUILD/voxprint-lucent" | grep -q "$lib"; then
        echo "viewer is not linked against $lib" >&2
        exit 1
    fi
done

STAGE=$(mktemp -d)
APPDIR=opt/voxprint-ai-suite/lucent
LIB="$STAGE/$APPDIR"
mkdir -p "$LIB/lib" "$LIB/bin" "$LIB/share" "$LIB/assets" \
    "$STAGE/usr/bin" "$STAGE/usr/share/applications" "$STAGE/DEBIAN"
cp "$BUILD/voxprint-lucent" "$LIB/voxprint-lucent"
cp "$ROOT/packaging/linux/qt.conf" "$LIB/qt.conf"
cp -a "$QT_PREFIX/lib/"*.so* "$LIB/lib/"
# qt.conf: Plugins = . so every plugin folder sits next to the binary.
for dir in "$QT_PREFIX/plugins/"*/; do
    cp -a "$dir" "$LIB/"
done
for plugin in platforms/libqxcb.so platforms/libqoffscreen.so imageformats/libqjpeg.so imageformats/libqwebp.so; do
    if [ ! -e "$LIB/$plugin" ]; then
        echo "release package is missing $plugin" >&2
        exit 1
    fi
done

# libavif and libjxl with their codec libraries. The C library, the C++
# runtime, and the dynamic loader stay with the system.
bundle_closure() {
    LD_LIBRARY_PATH="$LIB/lib" ldd "$1" | awk '/=> \//{print $1" "$3}' | while read -r name path; do
        case "$name" in
            libc.so*|libm.so*|libdl.so*|librt.so*|libpthread.so*|ld-linux*|libstdc++.so*|libgcc_s.so*) continue ;;
            libQt6*|libicu*) continue ;;
        esac
        if [ ! -e "$LIB/lib/$name" ]; then
            cp -L "$path" "$LIB/lib/$name"
            bundle_closure "$path"
        fi
    done
}
for name in $(LD_LIBRARY_PATH="$QT_PREFIX/lib" ldd "$BUILD/voxprint-lucent" | awk '/=> \//{print $1}' | grep -E '^lib(avif|jxl)'); do
    path=$(LD_LIBRARY_PATH="$QT_PREFIX/lib" ldd "$BUILD/voxprint-lucent" | awk -v n="$name" '$1==n{print $3}')
    cp -L "$path" "$LIB/lib/$name"
    bundle_closure "$path"
done
find "$LIB/lib" -name '*.a' -delete || true

cp -a "$ROOT/share/." "$LIB/share/"
if [ -f "$BUILD/share/lucent.qss" ]; then
    cp "$BUILD/share/lucent.qss" "$LIB/share/lucent.qss"
fi
cp "$ROOT/assets/voxprint-lucent-1024.png" "$LIB/assets/voxprint-lucent.png"

# Embedded Python 3.14 (python-build-standalone through uv) with the helper.
PYROOT=$(mktemp -d)
uv python install "$PYTHON_VERSION" --install-dir "$PYROOT" --no-bin
PYSRC=$(find "$PYROOT" -mindepth 1 -maxdepth 1 -type d -name 'cpython-3.14*' | head -n 1)
if [ -z "$PYSRC" ]; then
    echo "Python $PYTHON_VERSION was not installed" >&2
    exit 1
fi
cp -a "$PYSRC" "$LIB/python"
rm -rf "$PYROOT"
find "$LIB/python/lib" -maxdepth 2 -name EXTERNALLY-MANAGED -delete
PY="$LIB/python/bin/python3"
uv pip install --python "$PY" --break-system-packages --no-cache --compile-bytecode \
    "$COMMON_REQ" "PySide6-Essentials==6.12.0" "imagecodecs==2026.10.10" "numpy>=2.3"
uv pip install --python "$PY" --break-system-packages --no-cache --compile-bytecode --no-deps "$ROOT"
"$PY" -c "import voxprint_lucent.tools_host, voxprint_lucent.viewer_shell, voxprint_lucent.register, voxprint_common, imagecodecs; from PySide6 import QtWidgets"
rm -rf "$LIB/python/lib/python3.14/test" "$LIB/python/lib/python3.14/idlelib" "$LIB/python/lib/python3.14/tkinter" 2>/dev/null || true

# Helper interpreter for the viewer: Qt and plugin paths of the C++ runtime
# must not leak into PySide6.
cat > "$LIB/bin/lucent-python" << 'EOF2'
#!/bin/sh
unset LD_LIBRARY_PATH QT_PLUGIN_PATH PYTHONPATH PYTHONHOME
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
exec "$HERE/python/bin/python3" -s "$@"
EOF2

cat > "$STAGE/usr/bin/voxprint-lucent" << EOF2
#!/bin/sh
ROOT=\${LUCENT_ROOT:-/$APPDIR}
export LD_LIBRARY_PATH="\$ROOT/lib\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="\$ROOT"
export LUCENT_PYTHON="\${LUCENT_PYTHON:-\$ROOT/bin/lucent-python}"
# Record this install in the suite registry (state/installed-apps.json). Idempotent.
( "\$ROOT/bin/lucent-python" -m voxprint_lucent.register --path "\$ROOT" --version "$VERSION" >/dev/null 2>&1 & )
exec "\$ROOT/voxprint-lucent" "\$@"
EOF2
cp "$STAGE/usr/bin/voxprint-lucent" "$STAGE/usr/bin/voxprint-lucent-cli"
cat > "$STAGE/usr/bin/voxprint-lucent-tools" << EOF2
#!/bin/sh
ROOT=\${LUCENT_ROOT:-/$APPDIR}
exec "\$ROOT/bin/lucent-python" -m voxprint_lucent.tools_host "\$@"
EOF2
chmod 755 "$STAGE/usr/bin/voxprint-lucent" "$STAGE/usr/bin/voxprint-lucent-cli" \
    "$STAGE/usr/bin/voxprint-lucent-tools" "$LIB/voxprint-lucent" "$LIB/bin/lucent-python"

sed -e "s|^Exec=.*|Exec=/usr/bin/voxprint-lucent %F|" \
    -e "s|^X-Voxprint-Version=.*|X-Voxprint-Version=$VERSION|" \
    -e "s|^X-Voxprint-Build=.*|X-Voxprint-Build=$BUILD_NUMBER|" \
    "$ROOT/packaging/linux/ai.voxprint.Lucent.desktop" \
    > "$STAGE/usr/share/applications/ai.voxprint.Lucent.desktop"
for size in 16 24 32 48 64 128 256; do
    mkdir -p "$STAGE/usr/share/icons/hicolor/${size}x${size}/apps"
    cp "$ROOT/assets/icon-$size.png" "$STAGE/usr/share/icons/hicolor/${size}x${size}/apps/ai.voxprint.Lucent.png"
done

# libc6 floor = the newest GLIBC_x.y symbol version any staged ELF file needs.
GLIBC_MIN=$(find "$STAGE/opt" -type f \( -perm -u+x -o -name '*.so*' \) -print0 \
    | xargs -0 objdump -T 2>/dev/null \
    | grep -o 'GLIBC_[0-9][0-9.]*' | sed 's/GLIBC_//' | sort -uV | tail -n 1)
[ -n "$GLIBC_MIN" ] || { echo "could not read the glibc floor" >&2; exit 1; }
echo "libc6 floor: $GLIBC_MIN"

cat > "$STAGE/DEBIAN/control" << EOF2
Package: voxprint-lucent
Version: ${DEB_VERSION}
Section: graphics
Priority: optional
Architecture: amd64
Depends: libc6 (>= ${GLIBC_MIN}), libstdc++6, libegl1, libgl1, libglib2.0-0 | libglib2.0-0t64, libxkbcommon0, libxkbcommon-x11-0, libfontconfig1, libfreetype6, libdbus-1-3, libx11-6, libx11-xcb1, libxcb1, libxcb-cursor0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-shape0, libxcb-xfixes0, libxcb-xkb1
Installed-Size: $(du -sk "$STAGE" | cut -f1)
Maintainer: Aleksandr Mitroshenkov <223849179+Mitroshenkov87@users.noreply.github.com>
Description: VOXPrint AI Lucent image viewer
 Fast, colour-accurate image viewer for the VOXPrint AI Suite: AVIF, JPEG XL,
 WebP and JPEG. Does not need an NVIDIA GPU. Upscaling uses VOXPrint AI
 Enlarge when it is installed. Ships its own Qt 6.12 and Python 3.14 runtime.
EOF2
cat > "$STAGE/DEBIAN/postinst" << 'EOF2'
#!/bin/sh
set -e
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database -q /usr/share/applications || true; fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true; fi
exit 0
EOF2
cp "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
chmod 755 "$STAGE/DEBIAN/postinst" "$STAGE/DEBIAN/postrm"
printf 'version=%s\nbuild=%s\nqt=%s\npython=%s\nvoxprint-common=%s\n' \
    "$VERSION" "$BUILD_NUMBER" "$QT_VERSION" "$PYTHON_VERSION" "$COMMON_REQ" > "$LIB/VERSIONS.txt"

OUT="$ROOT/dist"
mkdir -p "$OUT"
dpkg-deb --root-owner-group -Zxz --build "$STAGE" "$OUT/voxprint-lucent_${FILE_VERSION}_amd64.deb"
rm -rf "$STAGE"
echo "Wrote $OUT/voxprint-lucent_${FILE_VERSION}_amd64.deb"
