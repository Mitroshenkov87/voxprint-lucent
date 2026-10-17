"""Preset merge and the close-edit rule, using the shared JSON fixtures."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from voxprint_lucent.presets import (
    ChipKind,
    chip_for,
    choose_option,
    edit_value,
    format_chip,
    load_preset_document,
    load_preset_file,
    merge_presets,
    owned_keys,
    reserved_setting_key,
    reset_group,
)

FIXTURES = Path(__file__).parent / "fixtures" / "shell"


def _load(name: str) -> object:
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


def _defaults() -> dict[str, object]:
    payload = _load("defaults.json")
    assert isinstance(payload, dict)
    return payload


def test_merge_writes_the_default_option_and_keeps_other_keys() -> None:
    document = load_preset_document(_load("quality.json"))
    state = merge_presets(document, _defaults())
    assert state.values["shell.steps"] == 20
    assert state.values["shell.guidance"] == 7.0
    assert state.values["shell.sampler"] == "euler"
    assert state.values["shell.note"] == "kept"
    status = chip_for(document, state, "quality")
    assert status.kind is ChipKind.EXACT
    assert format_chip(status.kind, "Balanced", "{name} + changes", "Custom") == "Balanced"


def test_close_edit_reads_name_plus_changes_and_reset_restores_it() -> None:
    document = load_preset_document(_load("quality.json"))
    stored = _load("stored-close.json")
    assert isinstance(stored, dict)
    state = merge_presets(document, _defaults(), stored)
    status = chip_for(document, state, "quality")
    assert status.kind is ChipKind.MODIFIED
    assert state.overrides["quality"] == ["shell.steps"]
    label = format_chip(status.kind, "Balanced", "{name} + changes", "Custom")
    assert label == "Balanced + changes"
    assert reset_group(document, state, "quality")
    assert state.values["shell.steps"] == 20
    assert state.values["shell.guidance"] == 7.0
    assert state.values["shell.sampler"] == "euler"
    assert state.values["shell.note"] == "kept"


def test_far_edit_reads_custom_and_reset_still_restores() -> None:
    document = load_preset_document(_load("quality.json"))
    stored = _load("stored-far.json")
    assert isinstance(stored, dict)
    state = merge_presets(document, _defaults(), stored)
    status = chip_for(document, state, "quality")
    assert status.kind is ChipKind.CUSTOM
    assert status.option_id == "balanced"
    assert format_chip(status.kind, "Balanced", "{name} + changes", "Custom") == "Custom"
    assert reset_group(document, state, "quality")
    assert state.values["shell.steps"] == 20
    assert state.values["shell.guidance"] == 7.0


def test_one_key_group_is_custom_after_a_single_change() -> None:
    document = load_preset_document(_load("one-key.json"))
    state = merge_presets(document, {})
    assert edit_value(document, state, "viewer.zoom", "free")
    assert chip_for(document, state, "zoom").kind is ChipKind.CUSTOM


def test_later_group_owns_a_shared_key() -> None:
    document = load_preset_document(_load("overlap.json"))
    state = merge_presets(document, _defaults())
    assert state.values["shell.steps"] == 30
    assert chip_for(document, state, "quality").kind is ChipKind.EXACT
    assert owned_keys(document, "quality") == ("shell.guidance", "shell.sampler")


def test_unavailable_option_does_not_write() -> None:
    document = load_preset_document(_load("unavailable.json"))
    state = merge_presets(document, {})
    before = state.values["shell.output"]
    assert choose_option(document, state, "output", "4k") is False
    assert state.values["shell.output"] == before


def test_reserved_keys_are_rejected_and_adult_content_is_kept() -> None:
    with pytest.raises(ValueError, match="reserved key"):
        load_preset_document(_load("safety.json"))
    assert reserved_setting_key("safety")
    assert reserved_setting_key("safety.gate")
    assert reserved_setting_key("image.minor")
    assert reserved_setting_key("image.minors")
    assert reserved_setting_key("crop.age")
    assert reserved_setting_key("viewer.guard")
    assert reserved_setting_key("image.adult_content") is False
    assert load_preset_file(FIXTURES / "adult-ok.json").group("image") is not None


def test_lucent_preset_writes_fit_and_enhance() -> None:
    document = load_preset_file(Path("share/presets/lucent.json"))
    state = merge_presets(document, {})
    assert state.values["viewer.zoom"] == "fit"
    assert state.values["enlarge.scale"] == 0
    assert choose_option(document, state, "view", "enhance")
    assert state.values["enlarge.scale"] == 2
    assert state.values["enlarge.preset"] == "photo"
    assert state.values["viewer.zoom"] == "fit"
