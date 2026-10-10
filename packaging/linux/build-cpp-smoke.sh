#!/bin/sh
# Build the C++ viewer against the Qt 6.12 SDK (aqtinstall) and require FIRST_PIXEL.
# Distro Qt on Ubuntu 24.04 is 6.4, which is too old for the product. This job
# does not build KImageFormats; AVIF and JPEG XL plugins are in the Windows harness.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
QT_VERSION=6.12.0
QT_ROOT=${RUNNER_TEMP:-/tmp}/lucent-qt
python -m pip install --upgrade aqtinstall
# Linux desktop Qt 6.12 is still at qt6_6120/qt6_6120, which aqtinstall 3.3.0 understands.
# --archives qtbase is enough for the Widgets smoke binary.
# icu-linux-*.7z is a separate archive in the same package. QtCore links ICU 73.
python -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 --outputdir "$QT_ROOT" --archives qtbase icu
QT_PREFIX=$(find "$QT_ROOT/$QT_VERSION" -maxdepth 2 -type d -name gcc_64 | head -n 1)
if [ -z "$QT_PREFIX" ]; then
    echo "Qt prefix not found under $QT_ROOT" >&2
    exit 1
fi

cmake -S "$ROOT/cpp" -B "$ROOT/cpp/build-ci" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$ROOT/cpp/build-ci"

python - <<'PY'
import base64
from pathlib import Path
blob = """/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAAgADADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDwSiiivz8/rsKKKKACiiigAooooAKKKKACiiigD//Z"""
Path("/tmp/lucent-smoke.jpg").write_bytes(base64.b64decode(blob))
PY

export LD_LIBRARY_PATH="$QT_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_QPA_PLATFORM=xcb
export LIBGL_ALWAYS_SOFTWARE=1
out=$(xvfb-run -a -s "-screen 0 1280x720x24" "$ROOT/cpp/build-ci/voxprint-lucent" /tmp/lucent-smoke.jpg --benchmark)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '^FIRST_PIXEL$'
