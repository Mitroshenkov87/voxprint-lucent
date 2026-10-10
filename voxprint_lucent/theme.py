"""Qt stylesheet built from the shared palette files."""

from __future__ import annotations

from voxprint_lucent.theme_tokens import render_stylesheet


def build_stylesheet() -> str:
    """Dark theme. The window border uses the approved splash colour."""
    return render_stylesheet()
