"""Lucent registers itself in the suite registry through voxprint-common."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from voxprint_lucent.register import main, register


def _rows(tmp_path: Path) -> list[dict[str, object]]:
    registry = tmp_path / "state" / "installed-apps.json"
    payload = json.loads(registry.read_text(encoding="utf-8"))
    return list(payload["apps"])


def test_register_twice_keeps_one_row(tmp_path: Path, monkeypatch) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    install_dir = tmp_path / "apps" / "lucent"
    install_dir.mkdir(parents=True)
    register(install_dir, "0.8.0")
    register(install_dir, "0.8.0")
    rows = _rows(tmp_path)
    assert len(rows) == 1
    assert rows[0]["id"] == "lucent"
    assert rows[0]["display_name"] == "VOXPrint AI Lucent"
    assert rows[0]["version"] == "0.8.0"


def test_unregister_removes_the_row(tmp_path: Path, monkeypatch) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    install_dir = tmp_path / "apps" / "lucent"
    install_dir.mkdir(parents=True)
    register(install_dir, "0.8.0")
    assert main(["--unregister"]) == 0
    assert _rows(tmp_path) == []
    # Unregistering again is a quiet no-op.
    assert main(["--unregister"]) == 0


def test_cli_register_is_silent_and_updates_version(
    tmp_path: Path, monkeypatch, capsys
) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    install_dir = tmp_path / "apps" / "lucent"
    install_dir.mkdir(parents=True)
    assert main(["--path", str(install_dir), "--version", "0.8.0"]) == 0
    assert capsys.readouterr().out == ""
    assert _rows(tmp_path)[0]["version"] == "0.8.0"
    assert main(["--path", str(install_dir), "--version", "0.9.0", "--verbose"]) == 0
    captured = capsys.readouterr()
    assert "registered lucent 0.9.0" in captured.out
    rows = _rows(tmp_path)
    assert len(rows) == 1
    assert rows[0]["version"] == "0.9.0"


def test_cli_error_prints_one_line_and_exits_1(monkeypatch, capsys) -> None:
    def boom(install_dir: Path, version: str) -> None:
        raise RuntimeError("catalog exploded")

    monkeypatch.setattr("voxprint_lucent.register.register", boom)
    code = main(["--path", "/opt/lucent", "--version", "0.8.0"])
    captured = capsys.readouterr()
    assert code == 1
    assert captured.out == ""
    assert captured.err.count("\n") == 1
    assert "catalog exploded" in captured.err


def test_module_entrypoint(tmp_path: Path, monkeypatch) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    install_dir = tmp_path / "apps" / "lucent"
    install_dir.mkdir(parents=True)
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent.register",
            "--path",
            str(install_dir),
            "--version",
            "0.8.0",
            "--verbose",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    assert "registered lucent" in proc.stdout
    assert len(_rows(tmp_path)) == 1
