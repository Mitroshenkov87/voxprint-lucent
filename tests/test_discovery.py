"""Enlarge is found from the shared suite manifest and the installer path."""

from __future__ import annotations

import json
import os
from pathlib import Path

from voxprint_lucent.discovery import find_app


def _isolate(monkeypatch, root: Path) -> None:
    empty = root / "empty"
    empty.mkdir()
    monkeypatch.setenv("VOXPRINT_HOME", str(root / "home"))
    monkeypatch.setenv("XDG_DATA_HOME", str(empty))
    monkeypatch.setenv("XDG_DATA_DIRS", str(empty))
    monkeypatch.setenv("PATH", str(empty))
    monkeypatch.delenv("LUCENT_ENLARGE_EXE", raising=False)


def test_manifest_apps_exe(tmp_path: Path, monkeypatch) -> None:
    _isolate(monkeypatch, tmp_path)
    home = tmp_path / "home"
    exe = home / "bin" / "VoxprintEnlarge.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("#!/bin/sh\n", encoding="utf-8")
    shared = home / "shared"
    shared.mkdir()
    (shared / "manifest.json").write_text(
        json.dumps(
            {
                "schema": 1,
                "apps": {
                    "enlarge": {
                        "exe": str(exe),
                        "protocol": 1,
                        "version": "0.2.0-rc.2",
                        "ipc": "voxprint-enlarge",
                    }
                },
            }
        ),
        encoding="utf-8",
    )
    found = find_app("Enlarge", "enlarge")
    assert found is not None
    assert found.command == [str(exe.resolve())]
    assert found.version == "0.2.0-rc.2"
    assert found.ipc_base == "voxprint-enlarge"


def test_manifest_holder_uses_the_installer_path(tmp_path: Path, monkeypatch) -> None:
    _isolate(monkeypatch, tmp_path)
    home = tmp_path / "home"
    exe = home / "enlarge" / "voxprint-enlarge"
    exe.parent.mkdir(parents=True)
    exe.write_text("#!/bin/sh\n", encoding="utf-8")
    shared = home / "shared"
    shared.mkdir()
    (shared / "manifest.json").write_text(
        json.dumps(
            {
                "schema": 1,
                "components": {"python": {"version": "3.14", "holders": ["enlarge"]}},
            }
        ),
        encoding="utf-8",
    )
    found = find_app("Enlarge", "enlarge")
    assert found is not None
    assert found.command == [str(exe.resolve())]


def test_installer_path_without_a_manifest(tmp_path: Path, monkeypatch) -> None:
    _isolate(monkeypatch, tmp_path)
    home = tmp_path / "home"
    exe = home / "enlarge" / "voxprint-enlarge"
    exe.parent.mkdir(parents=True)
    exe.write_text("#!/bin/sh\n", encoding="utf-8")
    found = find_app("Enlarge", "enlarge")
    assert found is not None
    assert found.command == [str(exe.resolve())]


def test_absent_enlarge_is_not_installed(tmp_path: Path, monkeypatch) -> None:
    _isolate(monkeypatch, tmp_path)
    (tmp_path / "home").mkdir()
    assert find_app("Enlarge", "enlarge") is None
    assert os.environ["VOXPRINT_HOME"] == str(tmp_path / "home")
