"""JSON label catalogs. English is the only language. This module does not import Qt."""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path

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
    """Resolve `key` from the English app catalog, then the English shell catalog.

    Lucent ships English only: `language` is kept for compatibility and any
    non-English value is ignored. A key missing from English is returned unchanged.
    """
    _ = language
    root = share_dir()
    app_en = _read(str(root / "locales" / "lucent" / "en.json"))
    shell_en = _read(str(root / "locales" / "en.json"))
    for table in (app_en, shell_en):
        if key in table:
            return table[key]
    return key
