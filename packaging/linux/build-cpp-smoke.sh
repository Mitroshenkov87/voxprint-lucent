#!/bin/sh
# Build the C++ viewer against the Qt 6.12 SDK (aqtinstall) and require FIRST_PIXEL.
# Distro Qt on Ubuntu 24.04 is 6.4, which is too old for the product. This job
# does not build KImageFormats; AVIF and JPEG XL plugins are in the Windows harness.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
c++ -std=c++20 -I "$ROOT/cpp/src" \
    "$ROOT/cpp/src/placement.cpp" \
    "$ROOT/cpp/src/natural.cpp" \
    "$ROOT/cpp/tests/m1_logic.cpp" \
    -o /tmp/m1_logic
/tmp/m1_logic

c++ -std=c++20 -I "$ROOT/cpp/src" \
    "$ROOT/cpp/tests/tone_curve_test.cpp" \
    -o /tmp/tone_curve_test
/tmp/tone_curve_test

QT_VERSION=6.12.0
QT_ROOT=${RUNNER_TEMP:-/tmp}/lucent-qt
QT_PREFIX="$QT_ROOT/$QT_VERSION/gcc_64"
# A restored actions/cache tree already has this prefix. Skip the Qt download.
if [ ! -e "$QT_PREFIX/lib/libQt6Core.so" ] && [ ! -e "$QT_PREFIX/lib/libQt6Core.so.6" ]; then
  python -m pip install --upgrade aqtinstall
  # Linux desktop Qt 6.12 is still at qt6_6120/qt6_6120, which aqtinstall 3.3.0 understands.
  # --archives qtbase is enough for the Widgets smoke binary.
  # icu-linux-*.7z is a separate archive in the same package. QtCore links ICU 73.
  python -m aqt install-qt linux desktop "$QT_VERSION" linux_gcc_64 --outputdir "$QT_ROOT" --archives qtbase icu
fi
if [ ! -d "$QT_PREFIX" ]; then
  QT_PREFIX=$(find "$QT_ROOT/$QT_VERSION" -maxdepth 2 -type d -name gcc_64 | head -n 1)
fi
if [ -z "$QT_PREFIX" ]; then
    echo "Qt prefix not found under $QT_ROOT" >&2
    exit 1
fi

set -- -S "$ROOT/cpp" -B "$ROOT/cpp/build-ci" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
# ccache does not apply to the MSVC harness. This smoke job is gcc/g++.
if command -v ccache >/dev/null 2>&1; then
  set -- "$@" -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
fi
cmake "$@"
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
# Accepted gates for v0.1.x:
#   cold first pixel <= 450 ms (Run-M0 cold_gate: JPEG, first launch after reboot)
#   warm JPEG and WebP <= 250 ms (this job, and Run-M0 warm_gate)
#   warm full-res AVIF and JPEG XL at 12 MP <= 350 ms (Run-M0 warm_gate)
# One unmeasured launch warms the loader. The timed launch enforces the warm
# figure on a tiny JPEG. This file has no embedded preview, so that pixel is
# the full decode. It is not a cold-boot sample.
WARM_FIRST_PIXEL_MS=250
xvfb-run -a -s "-screen 0 1280x720x24" "$ROOT/cpp/build-ci/voxprint-lucent" /tmp/lucent-smoke.jpg --benchmark >/dev/null
out=$(xvfb-run -a -s "-screen 0 1280x720x24" "$ROOT/cpp/build-ci/voxprint-lucent" /tmp/lucent-smoke.jpg --benchmark)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -q '^FIRST_PIXEL$'
phase=$(printf '%s\n' "$out" | awk '/^LUCENT_PHASE first_image_presented_ms /{print $3}')
if [ -z "$phase" ] || [ "$phase" -gt "$WARM_FIRST_PIXEL_MS" ]; then
    echo "warm first visible pixel regression: ${phase:-missing} ms (budget ${WARM_FIRST_PIXEL_MS})" >&2
    exit 1
fi

# After the timed launch. --dry-run uses QCoreApplication: no window, no GPU.
ver=$("$ROOT/cpp/build-ci/voxprint-lucent" --version)
printf '%s\n' "$ver"
printf '%s\n' "$ver" | grep -q '0.1.0 RC'
printf '%s\n' "$ver" | grep -q 'build 8'
printf '%s\n' "$ver" | grep -q Orah
absent=$("$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-smoke.jpg)
printf '%s\n' "$absent"
printf '%s\n' "$absent" | grep -q '"dry_run":true'
printf '%s\n' "$absent" | grep -q '"startup":"ok"'
printf '%s\n' "$absent" | grep -q '"installed":false'
printf '%s\n' "$absent" | grep -q 'github.com/Mitroshenkov87/voxprint-enlarge/releases'
stub=/tmp/lucent-enlarge-stub
cat > "$stub" <<'EOF'
#!/bin/sh
if [ "$1" = "--capabilities" ]; then
  printf '%s\n' '{"protocol":1,"app":"enlarge","version":"0.0.0","gpu":{"status":"none","reason":"no_nvidia"}}'
  exit 0
fi
printf '%s\n' '{"protocol":1,"type":"accepted","id":"cli","queue_pos":0}'
printf '%s\n' '{"protocol":1,"type":"done","id":"cli","output":"/tmp/lucent-would.avif","width":2,"height":2,"format":"avif","ms":0}'
exit 0
EOF
chmod +x "$stub"
present=$(LUCENT_ENLARGE_EXE="$stub" "$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-smoke.jpg)
printf '%s\n' "$present"
printf '%s\n' "$present" | grep -q '"installed":true'
printf '%s\n' "$present" | grep -q '"type":"done"'
rel=$(cd /tmp && LUCENT_ENLARGE_EXE="$stub" "$ROOT/cpp/build-ci/voxprint-lucent" --dry-run lucent-smoke.jpg)
printf '%s\n' "$rel"
printf '%s\n' "$rel" | grep -q '"image":"/tmp/lucent-smoke.jpg"'
suite=$(mktemp -d)
mkdir -p "$suite/shared"
printf '%s\n' "{\"schema\":1,\"apps\":{\"enlarge\":{\"exe\":\"$stub\",\"protocol\":1}}}" > "$suite/shared/manifest.json"
manifest=$(VOXPRINT_HOME="$suite" "$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-smoke.jpg)
printf '%s\n' "$manifest"
printf '%s\n' "$manifest" | grep -q '"installed":true'
rm -rf "$suite"
known=$(mktemp -d)
mkdir -p "$known/enlarge"
cp "$stub" "$known/enlarge/voxprint-enlarge"
chmod +x "$known/enlarge/voxprint-enlarge"
held=$(VOXPRINT_HOME="$known" "$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-smoke.jpg)
printf '%s\n' "$held"
printf '%s\n' "$held" | grep -q '"installed":true'
rm -rf "$known"
holders=$(mktemp -d)
mkdir -p "$holders/enlarge" "$holders/shared"
cp "$stub" "$holders/enlarge/voxprint-enlarge"
chmod +x "$holders/enlarge/voxprint-enlarge"
printf '%s\n' '{"schema":1,"components":{"python":{"version":"3.14","holders":["enlarge"]}}}' > "$holders/shared/manifest.json"
held_manifest=$(VOXPRINT_HOME="$holders" "$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-smoke.jpg)
printf '%s\n' "$held_manifest"
printf '%s\n' "$held_manifest" | grep -q '"installed":true'
rm -rf "$holders"
both=$("$ROOT/cpp/build-ci/voxprint-lucent" /tmp/lucent-smoke.jpg --benchmark --dry-run)
printf '%s\n' "$both" | grep -q '"dry_run":true'
if printf '%s\n' "$both" | grep -q '^FIRST_PIXEL$'; then
  echo "dry-run constructed the benchmark viewer" >&2
  exit 1
fi
set +e
"$ROOT/cpp/build-ci/voxprint-lucent" --dry-run /tmp/lucent-missing.jpg >/dev/null
missing=$?
set -e
if [ "$missing" -ne 3 ]; then
  echo "missing image exit $missing, expected 3" >&2
  exit 1
fi
"$ROOT/cpp/build-ci/voxprint-lucent" --help | grep -q -- '--dry-run'
"$ROOT/cpp/build-ci/voxprint-lucent" --help | grep -q -- '--selftest'
selftest=$(QT_QPA_PLATFORM=offscreen "$ROOT/cpp/build-ci/voxprint-lucent" --selftest --json)
printf '%s\n' "$selftest"
printf '%s\n' "$selftest" | python3 -c '
import json, sys
payload = json.load(sys.stdin)
assert payload["ok"] is True
names = {item["name"] for item in payload["checks"]}
missing = {"jpeg", "webp", "avif", "jpegxl", "vc_runtime"} - names
if missing:
    raise SystemExit("selftest missing " + ", ".join(sorted(missing)))
jpeg = next(item for item in payload["checks"] if item["name"] == "jpeg")
if not jpeg["required"] or not jpeg["ok"]:
    raise SystemExit("jpeg check failed")
'
qss="$ROOT/cpp/build-ci/share/lucent.qss"
test -f "$qss"
grep -q '#60a5fa' "$qss"
grep -q '#17171c' "$qss"
test ! -e "$ROOT/cpp/build-ci/share/palette.json"
PYTHONPATH="$ROOT" python3 "$ROOT/packaging/docs/generate_cli_md.py" \
  --viewer "$ROOT/cpp/build-ci/voxprint-lucent" --check
sh "$ROOT/scripts/vm_test.sh" --smoke --json \
  --viewer "$ROOT/cpp/build-ci/voxprint-lucent" \
  --image /tmp/lucent-smoke.jpg
