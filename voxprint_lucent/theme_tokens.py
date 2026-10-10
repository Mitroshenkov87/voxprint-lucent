"""Palette tokens loaded from the shared ``share/palette.json`` file."""

from __future__ import annotations

import json
import os
from pathlib import Path


def share_dir() -> Path:
    """Directory that holds palette.json and lucent.qss.in."""
    override = os.environ.get("LUCENT_SHARE")
    if override:
        return Path(override)
    module_dir = Path(__file__).resolve().parent
    candidates = (
        module_dir.parent / "share",
        module_dir / "share",
    )
    for candidate in candidates:
        if (candidate / "palette.json").is_file():
            return candidate
    return candidates[0]


def load_tokens() -> dict[str, str]:
    """Read the shared palette from share/palette.json."""
    path = share_dir() / "palette.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise TypeError(f"{path} must be a JSON object")
    return {str(key): str(value) for key, value in data.items()}


_TOKENS = load_tokens()

PRIMARY = _TOKENS["PRIMARY"]
PRIMARY_HOVER = _TOKENS["PRIMARY_HOVER"]
PRIMARY_BORDER = _TOKENS["PRIMARY_BORDER"]
TEXT_ON_PRIMARY = _TOKENS["TEXT_ON_PRIMARY"]
PRIMARY_DISABLED_BG = _TOKENS["PRIMARY_DISABLED_BG"]
PRIMARY_DISABLED_TEXT = _TOKENS["PRIMARY_DISABLED_TEXT"]
MENU_BG = _TOKENS["MENU_BG"]
MENU_SELECTION = _TOKENS["MENU_SELECTION"]
ACCENT = _TOKENS["ACCENT"]
ACCENT_HOVER = _TOKENS["ACCENT_HOVER"]
ACCENT_SOFT = _TOKENS["ACCENT_SOFT"]
ACCENT_STRONG = _TOKENS["ACCENT_STRONG"]
TEXT_ON_ACCENT = _TOKENS["TEXT_ON_ACCENT"]
SPLASH = _TOKENS["SPLASH"]
TEXT = _TOKENS["TEXT"]
TEXT_MUTED = _TOKENS["TEXT_MUTED"]
TEXT_FAINT = _TOKENS["TEXT_FAINT"]
TEXT_DISABLED = _TOKENS["TEXT_DISABLED"]
TEXT_ON_SELECTION = _TOKENS["TEXT_ON_SELECTION"]
ROOT_PLAIN = _TOKENS["ROOT_PLAIN"]
ROOT_GLASS = _TOKENS["ROOT_GLASS"]
ROOT_GLASS_MORE = _TOKENS["ROOT_GLASS_MORE"]
CARD_PLAIN = _TOKENS["CARD_PLAIN"]
CARD_GLASS = _TOKENS["CARD_GLASS"]
CARD_GLASS_MORE = _TOKENS["CARD_GLASS_MORE"]
CONTROL_BG = _TOKENS["CONTROL_BG"]
CONTROL_HOVER = _TOKENS["CONTROL_HOVER"]
CONTROL_BORDER = _TOKENS["CONTROL_BORDER"]
AMBER = _TOKENS["AMBER"]
NOTE_BG = _TOKENS["NOTE_BG"]
NOTE_BORDER = _TOKENS["NOTE_BORDER"]
GREEN = _TOKENS["GREEN"]
BADGE_GREEN_TEXT = _TOKENS["BADGE_GREEN_TEXT"]
BADGE_AMBER_TEXT = _TOKENS["BADGE_AMBER_TEXT"]
RED = _TOKENS["RED"]


def _channel(value: int) -> float:
    scaled = value / 255
    if scaled <= 0.04045:
        return scaled / 12.92
    return ((scaled + 0.055) / 1.055) ** 2.4


def _parse(hex_colour: str) -> tuple[int, int, int]:
    text = hex_colour.removeprefix("#")
    if len(text) != 6:
        raise ValueError(f"Expected #RRGGBB, got {hex_colour}")
    return int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16)


def relative_luminance(hex_colour: str) -> float:
    """Return the relative luminance of a #RRGGBB colour."""
    red, green, blue = _parse(hex_colour)
    return 0.2126 * _channel(red) + 0.7152 * _channel(green) + 0.0722 * _channel(blue)


def contrast_ratio(foreground: str, background: str) -> float:
    """WCAG contrast ratio. Both colours are opaque #RRGGBB."""
    lighter = relative_luminance(foreground)
    darker = relative_luminance(background)
    if darker > lighter:
        lighter, darker = darker, lighter
    return (lighter + 0.05) / (darker + 0.05)


def render_stylesheet(tokens: dict[str, str] | None = None) -> str:
    """Substitute palette tokens into ``share/lucent.qss.in``."""
    values = _TOKENS if tokens is None else tokens
    sheet = (share_dir() / "lucent.qss.in").read_text(encoding="utf-8")
    for name, value in values.items():
        sheet = sheet.replace(f"@{name}@", value)
    return sheet
