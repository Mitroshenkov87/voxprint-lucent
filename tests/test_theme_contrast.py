"""WCAG AA pairs for the suite palette. Disabled text is exempt."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from voxprint_lucent.theme import build_stylesheet
from voxprint_lucent.theme_tokens import (
    ACCENT,
    ACCENT_STRONG,
    AMBER,
    BADGE_AMBER_TEXT,
    BADGE_GREEN_TEXT,
    CARD_PLAIN,
    CONTROL_BG,
    GREEN,
    MENU_BG,
    MENU_SELECTION,
    NOTE_BG,
    PRIMARY,
    ROOT_PLAIN,
    SPLASH,
    TEXT,
    TEXT_FAINT,
    TEXT_MUTED,
    TEXT_ON_ACCENT,
    TEXT_ON_PRIMARY,
    TEXT_ON_SELECTION,
    contrast_ratio,
)

_PAIRS = (
    (TEXT, ROOT_PLAIN),
    (TEXT, CARD_PLAIN),
    (TEXT, MENU_BG),
    (TEXT, CONTROL_BG),
    (TEXT, NOTE_BG),
    (TEXT_MUTED, ROOT_PLAIN),
    (TEXT_MUTED, CARD_PLAIN),
    (TEXT_FAINT, ROOT_PLAIN),
    (TEXT_ON_SELECTION, MENU_SELECTION),
    (TEXT_ON_SELECTION, ACCENT_STRONG),
    (TEXT_ON_PRIMARY, PRIMARY),
    (TEXT_ON_ACCENT, ACCENT),
    (BADGE_GREEN_TEXT, GREEN),
    (BADGE_AMBER_TEXT, AMBER),
)


@pytest.mark.parametrize(("foreground", "background"), _PAIRS)
def test_text_pairs_meet_wcag_aa(foreground: str, background: str) -> None:
    assert contrast_ratio(foreground, background) >= 4.5


def test_stylesheet_uses_dark_ground_and_splash() -> None:
    sheet = build_stylesheet()
    assert ROOT_PLAIN in sheet
    assert PRIMARY in sheet
    assert SPLASH in sheet
    assert SPLASH == "#60a5fa"
    assert "QWidget#shell" in sheet
    assert "lucent-window" in sheet
    assert "2px solid #60a5fa" in sheet
    assert "@SPLASH@" not in sheet
    assert "@ROOT_PLAIN@" not in sheet
    assert "@FOCUS_RING_PX@" not in sheet


def test_vendored_tokens_match_the_pin() -> None:
    pin = json.loads(Path("share/tokens.pin.json").read_text(encoding="utf-8"))
    body = Path("share/tokens.json").read_bytes()
    assert pin["package"] == "voxprint-common"
    assert pin["pr"] == 13
    assert pin["commit"] == "8a94b531cb4f3526425a80a86a1e64624b5de606"
    tokens = json.loads(body)
    assert tokens["schema"] == 1
    assert "SPLASH" not in tokens["colors"]
    assert tokens["density"]["focus_ring_px"] == 2
    assert len(body) > 0
    assert "palette.json" not in {path.name for path in Path("share").iterdir()}
