"""BUILD.json is the source of the public build label."""

from __future__ import annotations

import json
from pathlib import Path

from voxprint_lucent.cli import load_build_payload, public_build_label


def test_build_json_names_tov() -> None:
    payload = json.loads(Path("BUILD.json").read_text(encoding="utf-8"))
    assert payload["codename"] == "Tov"
    assert payload["number"] == 7
    assert isinstance(payload["offset"], int)


def test_label_ignores_workflow_run_number(monkeypatch) -> None:
    monkeypatch.setenv("GITHUB_RUN_NUMBER", "99")
    payload = json.loads(Path("BUILD.json").read_text(encoding="utf-8"))
    expected = f'0.1.0 RC · build {payload["number"]} "{payload["codename"]}"'
    assert public_build_label() == expected


def test_label_walks_up_to_build_json(tmp_path: Path) -> None:
    (tmp_path / "BUILD.json").write_text(
        '{"number": 7, "offset": 0, "codename": "Tov"}\n',
        encoding="utf-8",
    )
    nested = tmp_path / "app" / "voxprint_lucent" / "cli.py"
    nested.parent.mkdir(parents=True)
    nested.write_text("", encoding="utf-8")
    payload = load_build_payload(nested)
    assert payload["codename"] == "Tov"
    assert payload["number"] == 7
