"""Labels for the tools window. English is the default.

The words come from the Lucent JSON catalogs. This module does not import Qt.
"""

from __future__ import annotations

import os

from voxprint_lucent.catalog import label as catalog_label
from voxprint_lucent.suite_state import ui_language

_KEYS = {
    "title": "lucent.tools.title",
    "info": "lucent.tools.info",
    "frames": "lucent.tools.frames",
    "play": "lucent.tools.play",
    "pause": "lucent.tools.pause",
    "prev": "lucent.tools.prev",
    "next": "lucent.tools.next",
    "export": "lucent.tools.export",
    "rotate": "lucent.tools.rotate",
    "flip_h": "lucent.tools.flip_h",
    "flip_v": "lucent.tools.flip_v",
    "crop": "lucent.tools.crop",
    "copy_path": "lucent.tools.copy_path",
    "copy_image": "lucent.tools.copy_image",
    "trash": "lucent.tools.trash",
    "reveal": "lucent.tools.reveal",
    "color": "lucent.tools.color",
}


def language() -> str:
    """Return the active tools-window language code."""
    override = os.environ.get("LUCENT_LANG", "").lower()[:2]
    if override in {"en", "ru", "de"}:
        return override
    code = ui_language()
    if code in {"en", "ru", "de"}:
        return code
    return "en"


def label(key: str) -> str:
    """Return the translated label for `key`."""
    return catalog_label(_KEYS[key], language())
