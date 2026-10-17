"""Atomic suite writes merge only the caller's keys."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from voxprint_lucent.suite_state import (
    data_home,
    read_document,
    read_expert_mode,
    suite_settings_path,
    write_expert_mode,
    write_own_keys,
)

FIXTURES = Path(__file__).parent / "fixtures" / "shell"


def test_suite_root_is_spelled_once() -> None:
    for name in ("voxprint_lucent/suite_state.py", "cpp/src/shell_logic.cpp"):
        text = Path(name).read_text(encoding="utf-8")
        assert text.count("voxprint-ai-suite") == 1
        assert text.count("VOXPrint AI Suite") == 1


def test_atomic_merge_keeps_foreign_keys_and_invalid_files(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    assert data_home() == tmp_path
    foreign = json.loads((FIXTURES / "suite-foreign.json").read_text(encoding="utf-8"))
    write_own_keys(suite_settings_path(), foreign)
    assert write_expert_mode(True) is True
    suite = read_document(suite_settings_path())
    assert suite["ui.expert_mode"] is True
    assert suite["ui_language"] == "de"
    assert suite["gpu"] == "auto"
    assert suite["keymap"]["schema"] == 1
    assert read_expert_mode() is True
    assert Path(str(suite_settings_path()) + ".lock").is_file()
    suite_settings_path().write_text("{", encoding="utf-8")
    kept = read_document(suite_settings_path())
    assert kept["ui_language"] == "de"
    write_expert_mode(False)
    repaired = read_document(suite_settings_path())
    assert repaired["ui.expert_mode"] is False
    assert repaired["gpu"] == "auto"


def test_reserved_suite_key_is_refused(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setenv("VOXPRINT_HOME", str(tmp_path))
    with pytest.raises(ValueError, match="reserved key"):
        write_own_keys(suite_settings_path(), {"safety.minor": False})
