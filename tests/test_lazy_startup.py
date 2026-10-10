"""The process entry must not import Qt or the codec stack until a window is needed."""

from __future__ import annotations

import subprocess
import sys


def test_startup_import_does_not_load_qt_or_codecs() -> None:
    script = (
        "import sys\n"
        "import voxprint_lucent.startup\n"
        "banned = ('PySide6', 'imagecodecs', 'numpy', 'voxprint_lucent.viewer', "
        "'voxprint_lucent.network_policy', 'voxprint_lucent.exporting', "
        "'voxprint_lucent.tools_host')\n"
        "loaded = [name for name in banned if name in sys.modules]\n"
        "print(loaded)\n"
        "raise SystemExit(0 if not loaded else 1)\n"
    )
    proc = subprocess.run(
        [sys.executable, "-c", script],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr + proc.stdout
