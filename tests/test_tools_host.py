"""The tools process stays answerable without opening a window."""

from __future__ import annotations

import subprocess
import sys


def test_tools_host_ping() -> None:
    proc = subprocess.run(
        [sys.executable, "-m", "voxprint_lucent.tools_host", "--ping"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    assert proc.stdout.strip() == "pong"
