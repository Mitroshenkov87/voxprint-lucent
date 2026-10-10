#!/bin/sh
# One command for the reference laptop (and any other machine).
# CI numbers are indicative. Pass/fail against 250 ms needs --reference-laptop
# on the owner's Windows 11 24H2 laptop, ideally after a reboot for a true cold run.
#
#   ./bench/run_local.sh
#   ./bench/run_local.sh --reference-laptop
#   ./bench/run_local.sh --nuitka
#   ./bench/run_local.sh --cold-after-reboot
#
# --cold-after-reboot runs a single measured launch per format (do this once
# after reboot, before opening other apps) and still reports the plan rule
# only when --reference-laptop is also set.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"
PY="${LUCENT_PYTHON:-python3.14}"
if [ ! -x "$ROOT/.venv/bin/python" ]; then
    "$PY" -m venv "$ROOT/.venv"
    "$ROOT/.venv/bin/python" -m pip install -e ".[dev,nuitka]"
fi
EXTRA=""
NUITKA=0
COLD_ONCE=0
for arg in "$@"; do
    case "$arg" in
        --nuitka) NUITKA=1 ;;
        --cold-after-reboot) COLD_ONCE=1 ;;
        *) EXTRA="$EXTRA $arg" ;;
    esac
done
EXE_ARGS=""
if [ "$NUITKA" -eq 1 ]; then
    "$ROOT/.venv/bin/python" -m nuitka \
        --standalone \
        --assume-yes-for-downloads \
        --enable-plugin=pyside6 \
        --output-dir="$ROOT/build/nuitka" \
        --output-filename=voxprint-lucent \
        "$ROOT/voxprint_lucent/__main__.py"
    BIN=$(find "$ROOT/build/nuitka" -type f -name 'voxprint-lucent' -o -name 'voxprint-lucent.bin' | head -n 1)
    EXE_ARGS="--executable $BIN"
fi
if [ "$COLD_ONCE" -eq 1 ]; then
    # shellcheck disable=SC2086
    exec "$ROOT/.venv/bin/python" -m bench.harness --runs 1 --profiles cold $EXE_ARGS $EXTRA
fi
# shellcheck disable=SC2086
exec "$ROOT/.venv/bin/python" -m bench.harness --runs 10 --profiles warm,cold $EXE_ARGS $EXTRA
