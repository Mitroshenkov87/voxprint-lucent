"""WCAG AA pairs for the suite palette. Disabled text is exempt."""

from __future__ import annotations

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
    assert "lucent-window" in sheet
