#!/bin/sh
# Smoke-test a built voxprint-lucent .deb without a display.
#   sh packaging/linux/smoke-deb.sh dist/voxprint-lucent_<version>_amd64.deb [--install]
# --install installs it with apt (root/sudo); otherwise it is unpacked into a
# temporary prefix and run from there. Checks: viewer opens WebP, AVIF, JPEG XL
# and JPEG offscreen (FIRST_PIXEL), the GUI stays up for a few seconds, the
# selftest sees AVIF and JPEG XL, the Python helper starts, the suite registry
# row is written, and Enlarge reports not installed without failing.
set -eu
DEB=$1
MODE=${2:-}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
if [ "$MODE" = "--install" ]; then
    sudo apt-get install -y --no-install-recommends "$(realpath "$DEB")"
    APP=/opt/voxprint-ai-suite/lucent
    LAUNCH=/usr/bin/voxprint-lucent
    test -x "$LAUNCH"
    test -f /usr/share/applications/ai.voxprint.Lucent.desktop
    test -f /usr/share/icons/hicolor/256x256/apps/ai.voxprint.Lucent.png
else
    dpkg-deb -x "$DEB" "$WORK/root"
    APP="$WORK/root/opt/voxprint-ai-suite/lucent"
    LAUNCH="$WORK/root/usr/bin/voxprint-lucent"
    export LUCENT_ROOT="$APP"
fi
export VOXPRINT_HOME="$WORK/home"
export QT_QPA_PLATFORM=offscreen
PY="$APP/bin/lucent-python"

"$PY" - "$WORK/img" <<'PY'
import sys
from pathlib import Path
import imagecodecs
import numpy as np
root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
img = np.zeros((96, 128, 3), np.uint8)
img[:, :, 0] = 200
img[:, 64:, 1] = 180
root.joinpath("sample.webp").write_bytes(bytes(imagecodecs.webp_encode(img, level=80)))
root.joinpath("sample.avif").write_bytes(bytes(imagecodecs.avif_encode(img, level=60, speed=8)))
root.joinpath("sample.jxl").write_bytes(bytes(imagecodecs.jpegxl_encode(img, level=90, effort=3)))
root.joinpath("sample.jpg").write_bytes(bytes(imagecodecs.jpeg_encode(img, level=90)))
PY

for name in sample.webp sample.avif sample.jxl sample.jpg; do
    out=$(timeout 60 "$LAUNCH" "$WORK/img/$name" --benchmark 2>&1)
    if ! printf '%s\n' "$out" | grep -q '^FIRST_PIXEL$'; then
        printf '%s\n' "$out" >&2
        echo "FAIL: $name did not reach FIRST_PIXEL" >&2
        exit 1
    fi
    echo "ok: $name FIRST_PIXEL"
done

for name in sample.avif sample.jxl; do
    set +e
    timeout 6 "$LAUNCH" "$WORK/img/$name" >"$WORK/gui.log" 2>&1
    code=$?
    set -e
    if [ "$code" -ne 124 ]; then
        cat "$WORK/gui.log" >&2
        echo "FAIL: viewer window for $name exited with $code before the timeout" >&2
        exit 1
    fi
    echo "ok: viewer window stayed up with $name"
done

selftest=$("$LAUNCH" --selftest --json)
printf '%s\n' "$selftest" | "$PY" -c '
import json, sys
payload = json.load(sys.stdin)
checks = {item["name"]: item for item in payload["checks"]}
for name in ("jpeg", "webp", "avif", "jpegxl"):
    if not checks.get(name, {}).get("ok"):
        raise SystemExit("selftest: " + name + " not ok: " + json.dumps(checks.get(name)))
print("ok: selftest jpeg webp avif jpegxl")
'

dry=$("$LAUNCH" --dry-run "$WORK/img/sample.webp")
printf '%s\n' "$dry" | grep -q '"installed":false'
echo "ok: Enlarge absent, viewer still works"

"$PY" -m voxprint_lucent.tools_host --ping
echo "ok: tools helper"

sleep 2
"$PY" -m voxprint_lucent.register --path "$APP" --version smoke
"$PY" - "$VOXPRINT_HOME" "$APP" <<'PY'
import json, sys
from pathlib import Path
data = json.loads((Path(sys.argv[1]) / "state" / "installed-apps.json").read_text())
rows = [row for row in data["apps"] if row["id"] == "lucent"]
assert len(rows) == 1, data
assert Path(rows[0]["path"]) == Path(sys.argv[2]), rows
print("ok: registered in state/installed-apps.json")
PY
echo "SMOKE PASSED"
