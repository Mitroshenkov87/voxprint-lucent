"""Preset documents and the live settings state.

A preset file is schema ``voxprint-presets/1``. Merge order is the shell spec:
defaults, then groups in file order, then stored overrides. The chip reads the
option name, ``{name} + changes``, or ``Custom``.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Any

PRESET_SCHEMA = "voxprint-presets/1"
STATE_SCHEMA = 1
Value = bool | int | float | str


class ChipKind(Enum):
    """How the live values relate to the stored option."""

    EXACT = "exact"
    MODIFIED = "modified"
    CUSTOM = "custom"


@dataclass(frozen=True)
class PresetOption:
    """One chip and the values it writes, in file order."""

    option_id: str
    label_key: str
    values: tuple[tuple[str, Value], ...]
    available: bool = True
    reason_key: str = ""
    action_label_key: str = ""
    action_id: str = ""

    def as_map(self) -> dict[str, Value]:
        """Return the values in file order."""
        return dict(self.values)


@dataclass(frozen=True)
class PresetGroup:
    """One row of chips. Every option writes the same keys."""

    group_id: str
    label_key: str
    default: str
    options: tuple[PresetOption, ...]

    def option(self, option_id: str) -> PresetOption | None:
        """Return the option, or None when the id is unknown."""
        return next((item for item in self.options if item.option_id == option_id), None)

    @property
    def keys(self) -> tuple[str, ...]:
        """Return the keys the first option writes, in file order."""
        return tuple(key for key, _value in self.options[0].values)


@dataclass(frozen=True)
class PresetFile:
    """A validated preset document."""

    groups: tuple[PresetGroup, ...]

    def group(self, group_id: str) -> PresetGroup | None:
        """Return a group, or None."""
        return next((item for item in self.groups if item.group_id == group_id), None)


@dataclass(frozen=True)
class ChipStatus:
    """The chip label for one group."""

    kind: ChipKind
    option_id: str
    label_key: str


@dataclass
class SettingState:
    """Live values, the chosen option, and the keys that differ."""

    values: dict[str, Value] = field(default_factory=dict)
    chosen: dict[str, str] = field(default_factory=dict)
    overrides: dict[str, list[str]] = field(default_factory=dict)


def reserved_setting_key(key: str) -> bool:
    """Return whether ``key`` would configure the minors gate.

    A key is reserved when it is ``safety``, starts with ``safety.``, contains
    a ``safety`` segment, or its final segment is ``minor``, ``minors``,
    ``age``, or ``guard``. ``image.adult_content`` is not reserved.
    """
    folded = key.lower()
    if folded == "safety" or folded.startswith("safety."):
        return True
    if any(segment.lower() == "safety" for segment in key.split(".")):
        return True
    last = key.rsplit(".", 1)[-1].lower()
    return last in {"minor", "minors", "age", "guard"}


def load_preset_document(payload: object) -> PresetFile:
    """Validate a preset document.

    Raises:
        ValueError: The schema, a group, or a key is invalid.
    """
    if not isinstance(payload, dict) or payload.get("schema") != PRESET_SCHEMA:
        raise ValueError(f"presets schema must be {PRESET_SCHEMA}")
    raw_groups = payload.get("groups")
    if not isinstance(raw_groups, list) or not raw_groups:
        raise ValueError("presets need at least one group")
    groups: list[PresetGroup] = []
    seen: set[str] = set()
    for raw in raw_groups:
        group = _group(raw)
        if group.group_id in seen:
            raise ValueError(f"preset group {group.group_id} is repeated")
        seen.add(group.group_id)
        groups.append(group)
    return PresetFile(tuple(groups))


def load_preset_file(path: Path) -> PresetFile:
    """Read and validate a preset file.

    Raises:
        ValueError: The file is missing or invalid.
    """
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"presets file {path} is not valid JSON") from exc
    return load_preset_document(payload)


def set_option_available(
    document: PresetFile, group_id: str, option_id: str, *, available: bool
) -> PresetFile:
    """Return a copy with one option's availability changed.

    Raises:
        ValueError: The option is unknown, or hiding it would drop its reason.
    """
    groups: list[PresetGroup] = []
    found = False
    for group in document.groups:
        if group.group_id != group_id:
            groups.append(group)
            continue
        options: list[PresetOption] = []
        for option in group.options:
            if option.option_id != option_id:
                options.append(option)
                continue
            found = True
            missing_reason = not (
                option.reason_key and option.action_label_key and option.action_id
            )
            if not available and missing_reason:
                raise ValueError(f"preset option {option_id} needs a plain reason and an action")
            options.append(
                PresetOption(
                    option.option_id,
                    option.label_key,
                    option.values,
                    available,
                    option.reason_key,
                    option.action_label_key,
                    option.action_id,
                )
            )
        groups.append(PresetGroup(group.group_id, group.label_key, group.default, tuple(options)))
    if not found:
        raise ValueError(f"unknown preset {group_id}={option_id}")
    return PresetFile(tuple(groups))


def owned_keys(document: PresetFile, group_id: str) -> tuple[str, ...]:
    """Return the keys ``group_id`` owns.

    A key belongs to the last group in the file that writes it.
    """
    group = document.group(group_id)
    if group is None:
        return ()
    later: set[str] = set()
    seen = False
    for item in document.groups:
        if item.group_id == group_id:
            seen = True
            continue
        if seen:
            later.update(item.keys)
    return tuple(key for key in group.keys if key not in later)


def merge_presets(
    document: PresetFile,
    defaults: dict[str, Value],
    stored: dict[str, Any] | None = None,
) -> SettingState:
    """Build state from defaults, then groups, then a stored schema-1 document.

    An invalid stored document, including one with a reserved key, is skipped.
    """
    state = SettingState(values=dict(defaults))
    for group in document.groups:
        state.chosen[group.group_id] = group.default
        state.overrides[group.group_id] = []
        option = group.option(group.default)
        if option is None or not option.available:
            continue
        for key, value in option.values:
            state.values[key] = value
    if stored is not None and _apply_stored(document, stored, state):
        pass
    _recompute(document, state)
    return state


def chip_for(document: PresetFile, state: SettingState, group_id: str) -> ChipStatus:
    """Classify one group."""
    group = document.group(group_id)
    if group is None:
        return ChipStatus(ChipKind.CUSTOM, "", "")
    owned = owned_keys(document, group_id)
    for option in group.options:
        if option.available and _distance(option, owned, state) == 0:
            return ChipStatus(ChipKind.EXACT, option.option_id, option.label_key)
    stored_id = state.chosen.get(group_id, group.default)
    stored = group.option(stored_id) or group.option(group.default)
    if stored is None:
        return ChipStatus(ChipKind.CUSTOM, "", "")
    distance = _distance(stored, owned, state)
    limit = len(owned) // 2
    kind = ChipKind.MODIFIED if distance > 0 and distance <= limit else ChipKind.CUSTOM
    return ChipStatus(kind, stored.option_id, stored.label_key)


def format_chip(kind: ChipKind, name: str, changed_pattern: str, custom_word: str) -> str:
    """Return the option name, ``{name} + changes``, or the custom word."""
    if kind is ChipKind.EXACT:
        return name
    if kind is ChipKind.MODIFIED:
        return changed_pattern.replace("{name}", name)
    return custom_word


def choose_option(document: PresetFile, state: SettingState, group_id: str, option_id: str) -> bool:
    """Write one option. An unavailable or unknown option writes nothing."""
    group = document.group(group_id)
    option = None if group is None else group.option(option_id)
    if group is None or option is None or not option.available:
        return False
    for key, value in option.values:
        if reserved_setting_key(key):
            return False
        state.values[key] = value
    state.chosen[group_id] = option_id
    state.overrides[group_id] = []
    _recompute(document, state)
    return True


def edit_value(document: PresetFile, state: SettingState, key: str, value: Value) -> bool:
    """Hand-edit one key. A reserved key is refused."""
    if reserved_setting_key(key) or not isinstance(value, bool | int | float | str):
        return False
    state.values[key] = value
    _recompute(document, state)
    return True


def reset_group(document: PresetFile, state: SettingState, group_id: str) -> bool:
    """Restore the stored option. Keys that option does not write stay."""
    group = document.group(group_id)
    if group is None:
        return False
    option = group.option(state.chosen.get(group_id, group.default))
    if option is None:
        return False
    for key, value in option.values:
        state.values[key] = value
    state.overrides[group_id] = []
    _recompute(document, state)
    return True


def _recompute(document: PresetFile, state: SettingState) -> None:
    for group in document.groups:
        status = chip_for(document, state, group.group_id)
        state.chosen[group.group_id] = status.option_id
        if status.kind is ChipKind.EXACT:
            state.overrides[group.group_id] = []
            continue
        option = group.option(status.option_id)
        changed: list[str] = []
        if option is not None:
            values = option.as_map()
            for key in owned_keys(document, group.group_id):
                if not _same(state.values.get(key), values.get(key)):
                    changed.append(key)
        state.overrides[group.group_id] = changed


def _apply_stored(document: PresetFile, stored: dict[str, Any], state: SettingState) -> bool:
    if stored.get("schema") != STATE_SCHEMA:
        return False
    values = stored.get("values")
    chosen = stored.get("chosen")
    if not isinstance(values, dict) or not isinstance(chosen, dict):
        return False
    loaded: dict[str, Value] = {}
    for key, raw in values.items():
        allowed = isinstance(raw, bool | int | float | str)
        if not isinstance(key, str) or reserved_setting_key(key) or not allowed:
            return False
        if isinstance(raw, bool) or isinstance(raw, int | float | str):
            loaded[key] = raw
        else:
            return False
    for group in document.groups:
        option_id = chosen.get(group.group_id)
        if not isinstance(option_id, str) or group.option(option_id) is None:
            return False
    overrides = stored.get("overrides")
    if isinstance(overrides, dict):
        for keys in overrides.values():
            if not isinstance(keys, list):
                continue
            for key in keys:
                if isinstance(key, str) and reserved_setting_key(key):
                    return False
    state.values.update(loaded)
    for group in document.groups:
        option_id = chosen[group.group_id]
        if isinstance(option_id, str):
            state.chosen[group.group_id] = option_id
    return True


def _distance(option: PresetOption, owned: tuple[str, ...], state: SettingState) -> int:
    values = option.as_map()
    return sum(1 for key in owned if not _same(state.values.get(key), values[key]))


def _same(left: object, right: object) -> bool:
    if isinstance(left, bool) or isinstance(right, bool):
        return left == right and isinstance(left, bool) and isinstance(right, bool)
    if isinstance(left, int | float) and isinstance(right, int | float):
        return abs(float(left) - float(right)) < 1e-6
    return left == right


def _group(raw: object) -> PresetGroup:
    if not isinstance(raw, dict):
        raise ValueError("a preset group must be an object")
    group_id = raw.get("id")
    label_key = raw.get("label_key")
    default = raw.get("default")
    raw_options = raw.get("options")
    named = isinstance(group_id, str) and group_id and isinstance(label_key, str) and label_key
    if not named:
        raise ValueError("a preset group needs an id and a label")
    if not isinstance(default, str) or not isinstance(raw_options, list) or not raw_options:
        raise ValueError(f"preset group {group_id} needs a default and options")
    options = tuple(_option(item) for item in raw_options)
    ids = [item.option_id for item in options]
    if len(ids) != len(set(ids)):
        raise ValueError(f"preset group {group_id} repeats an option")
    keys = set(options[0].as_map())
    if any(set(item.as_map()) != keys for item in options):
        raise ValueError(f"preset group {group_id} options must write the same keys")
    if default not in ids:
        raise ValueError(f"preset group {group_id} default is not an option")
    return PresetGroup(group_id, label_key, default, options)


def _option(raw: object) -> PresetOption:
    if not isinstance(raw, dict):
        raise ValueError("a preset option must be an object")
    option_id = raw.get("id")
    label_key = raw.get("label_key")
    values = raw.get("values")
    named = isinstance(option_id, str) and option_id and isinstance(label_key, str) and label_key
    if not named:
        raise ValueError("a preset option needs an id and a label")
    if not isinstance(values, dict) or not values:
        raise ValueError(f"preset option {option_id} needs values")
    available = raw.get("available", True)
    reason_key = raw.get("reason_key", "")
    action_label_key = raw.get("action_label_key", "")
    action_id = raw.get("action_id", "")
    if not isinstance(available, bool):
        raise ValueError(f"preset option {option_id} has a bad availability")
    texts = (reason_key, action_label_key, action_id)
    if not all(isinstance(item, str) for item in texts):
        raise ValueError(f"preset option {option_id} has a bad action")
    if not available and (not reason_key or not action_label_key or not action_id):
        raise ValueError(f"preset option {option_id} needs a plain reason and an action")
    coerced: list[tuple[str, Value]] = []
    for key, value in values.items():
        if not isinstance(key, str) or not key:
            raise ValueError(f"preset option {option_id} has a bad key")
        if reserved_setting_key(key):
            raise ValueError(f"reserved key {key}")
        if not isinstance(value, bool | int | float | str):
            raise ValueError(f"preset option {option_id} has a bad value")
        coerced.append((key, value))
    return PresetOption(
        option_id, label_key, tuple(coerced), available, reason_key, action_label_key, action_id
    )
