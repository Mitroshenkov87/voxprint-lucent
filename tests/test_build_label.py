"""BUILD.json is the source of the public build label."""

from __future__ import annotations

import json
from pathlib import Path

from voxprint_lucent.cli import public_build_label


def test_build_json_names_zohar() -> None:
    payload = json.loads(Path("BUILD.json").read_text(encoding="utf-8"))
    assert payload["codename"] == "Zohar"
    assert isinstance(payload["offset"], int)


def test_label_uses_offset_when_no_run_number(monkeypatch) -> None:
    monkeypatch.delenv("GITHUB_RUN_NUMBER", raising=False)
    payload = json.loads(Path("BUILD.json").read_text(encoding="utf-8"))
    expected = f'0.1.0 RC · build {payload["offset"]} "{payload["codename"]}"'
    assert public_build_label() == expected
