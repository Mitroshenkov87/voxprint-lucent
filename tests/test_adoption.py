"""The helper pins voxprint-common and the viewer surface."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from voxprint_lucent.suite_state import app_settings_path
from voxprint_lucent.viewer_shell import SURFACE_ID

PIN = "0d1e30de68cc49549a2a94cb3695e5601c17e7db"
DEPENDENCY = (
    "voxprint-common[ui] @ git+https://github.com/Mitroshenkov87/voxprint-common@" + PIN
)


def test_helper_depends_on_the_pinned_ui_extra() -> None:
    assert DEPENDENCY in Path("pyproject.toml").read_text(encoding="utf-8")


def test_cpp_reads_tokens_and_locales_from_the_pinned_paths() -> None:
    pin = json.loads(Path("share/voxprint-common.pin.json").read_text(encoding="utf-8"))
    assert pin["commit"] == PIN
    assert pin["paths"]["tokens"] == "src/voxprint_common/ui/tokens.json"
    assert pin["paths"]["locales"] == "src/voxprint_common/ui/locales"
    assert Path("share/tokens.json").is_file()
    for code in ("en", "ru", "de", "uk", "lv"):
        assert (Path("share/locales") / f"{code}.json").is_file()
    header = Path("cpp/src/shell_logic.h").read_text(encoding="utf-8")
    assert 'kSurfaceId[] = "viewer"' in header


def test_viewer_surface_id_and_settings_directory() -> None:
    assert SURFACE_ID == "viewer"
    path = app_settings_path()
    assert path.parts[-3:] == ("state", "viewer", "image-settings.json")


def test_tools_host_import_does_not_load_the_shell_package() -> None:
    script = (
        "import sys\n"
        "import voxprint_lucent.tools_host\n"
        "banned = ('PySide6', 'voxprint_common', 'voxprint_lucent.viewer_shell')\n"
        "loaded = [name for name in banned if name in sys.modules]\n"
        "raise SystemExit(0 if not loaded else 1)\n"
    )
    proc = subprocess.run(
        [sys.executable, "-c", script],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr + proc.stdout
