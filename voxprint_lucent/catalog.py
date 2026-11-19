"""JSON label catalogs. English is the fallback. This module does not import Qt."""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path

from voxprint_lucent.suite_state import ui_language
from voxprint_lucent.theme_tokens import share_dir


@lru_cache(maxsize=16)
def _read(path: str) -> dict[str, str]:
    file = Path(path)
    if not file.is_file():
        return {}
    payload = json.loads(file.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        return {}
    return {str(key): value for key, value in payload.items() if isinstance(value, str)}


def label(key: str, language: str | None = None) -> str:
    """Resolve `key` from the app catalog, then the shell catalog, then English.

    A key missing from English is returned unchanged.
    """
    code = (language or ui_language() or "en").split("-", 1)[0].lower()
    root = share_dir()
    app = _read(str(root / "locales" / "lucent" / f"{code}.json"))
    shell = _read(str(root / "locales" / f"{code}.json"))
    app_en = _read(str(root / "locales" / "lucent" / "en.json"))
    shell_en = _read(str(root / "locales" / "en.json"))
    for table in (app, shell, app_en, shell_en):
        if key in table:
            return table[key]
    return key
