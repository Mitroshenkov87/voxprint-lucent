#!/bin/sh
# Non-interactive Lucent check.
# Exit 0 pass, 1 a check failed, 2 setup, 3 no GPU.
# Viewing does not need a GPU. Exit 3 only when --require-gpu is set and no
# GPU is present. Otherwise a missing GPU is recorded in the JSON and a
# passing run still exits 0.
# --smoke is the short pass (version, self-test, dry-run, one JPEG first
# pixel). It is the default. Budget: about 10 minutes.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
json=0
require_gpu=0
viewer=""
image=""

while [ $# -gt 0 ]; do
  case "$1" in
    --json) json=1; shift ;;
    --smoke) shift ;;
    --require-gpu) require_gpu=1; shift ;;
    --viewer)
      viewer=${2-}
      if [ -z "$viewer" ]; then
        echo "vm_test: --viewer needs a path" >&2
        exit 2
      fi
      shift 2
      ;;
    --image)
      image=${2-}
      if [ -z "$image" ]; then
        echo "vm_test: --image needs a path" >&2
        exit 2
      fi
      shift 2
      ;;
    -h|--help)
      echo "Usage: vm_test.sh [--json] [--smoke] [--require-gpu] [--viewer PATH] [--image PATH]"
      echo "Exit 0 pass, 1 test failed, 2 setup, 3 no GPU (only with --require-gpu)."
      exit 0
      ;;
    *)
      echo "vm_test: unknown argument $1" >&2
      exit 2
      ;;
  esac
done

gpu_present=false
if command -v nvidia-smi >/dev/null 2>&1 && nvidia-smi -L >/dev/null 2>&1; then
  gpu_present=true
fi

if [ -z "$viewer" ]; then
  if [ -n "${LUCENT_VIEWER:-}" ]; then
    viewer=$LUCENT_VIEWER
  elif [ -x "$ROOT/cpp/build-ci/voxprint-lucent" ]; then
    viewer=$ROOT/cpp/build-ci/voxprint-lucent
  fi
fi

report() {
  ok=$1
  code=$2
  detail=$3
  if [ "$json" -eq 1 ]; then
    python3 - "$ok" "$code" "$detail" "$gpu_present" "$require_gpu" <<'PY'
import json, sys
ok, code, detail, gpu_present, require_gpu = sys.argv[1:]
gpu_code = 0 if gpu_present == "true" else 3
print(json.dumps({
    "ok": ok == "true",
    "smoke": True,
    "exit": int(code),
    "detail": detail,
    "gpu": {
        "present": gpu_present == "true",
        "required": require_gpu == "1",
        "code": gpu_code,
    },
}, separators=(",", ":")))
PY
  else
    echo "vm_test: $detail"
  fi
}

if [ -z "$viewer" ] || [ ! -x "$viewer" ]; then
  report false 2 "viewer missing"
  exit 2
fi

if [ "$require_gpu" -eq 1 ] && [ "$gpu_present" = false ]; then
  report false 3 "no GPU"
  exit 3
fi

if [ ! -f "$ROOT/BUILD.json" ]; then
  report false 2 "BUILD.json missing"
  exit 2
fi

number=$(sed -n 's/.*"number" *: *\([0-9][0-9]*\).*/\1/p' "$ROOT/BUILD.json")
codename=$(sed -n 's/.*"codename" *: *"\([^"]*\)".*/\1/p' "$ROOT/BUILD.json")
if [ -z "$number" ] || [ -z "$codename" ]; then
  report false 2 "BUILD.json has no number or codename"
  exit 2
fi

if [ -z "$image" ]; then
  image=/tmp/lucent-vm-smoke.jpg
fi
if [ ! -f "$image" ]; then
  python3 - "$image" <<'PY'
import base64, sys
from pathlib import Path
blob = """/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAAgADADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDwSiiivz8/rsKKKKACiiigAooooAKKKKACiiigD//Z"""
Path(sys.argv[1]).write_bytes(base64.b64decode(blob))
PY
fi
if [ ! -f "$image" ]; then
  report false 2 "smoke image missing"
  exit 2
fi

fail() {
  report false 1 "$1"
  exit 1
}

version=$(QT_QPA_PLATFORM=offscreen "$viewer" --version 2>/dev/null || true)
printf '%s\n' "$version" | grep -q "build ${number}" || fail "version missing build ${number}"
printf '%s\n' "$version" | grep -q "$codename" || fail "version missing ${codename}"

selftest=$(QT_QPA_PLATFORM=offscreen "$viewer" --selftest --json 2>/dev/null || true)
printf '%s\n' "$selftest" | python3 -c 'import json,sys; assert json.load(sys.stdin)["ok"] is True' \
  || fail "selftest"

dry=$(QT_QPA_PLATFORM=offscreen "$viewer" --dry-run "$image" 2>/dev/null || true)
printf '%s\n' "$dry" | grep -q '"dry_run":true' || fail "dry-run"

if [ -n "${DISPLAY:-}" ]; then
  bench=$("$viewer" "$image" --benchmark 2>/dev/null || true)
elif command -v xvfb-run >/dev/null 2>&1; then
  bench=$(xvfb-run -a -s "-screen 0 1280x720x24" "$viewer" "$image" --benchmark 2>/dev/null || true)
else
  bench=$(QT_QPA_PLATFORM=offscreen "$viewer" "$image" --benchmark 2>/dev/null || true)
fi
printf '%s\n' "$bench" | grep -q '^FIRST_PIXEL$' || fail "first pixel"

report true 0 "pass"
exit 0
