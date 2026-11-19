"""The tools window shows read-only facts and the shared palette."""

from __future__ import annotations

from pathlib import Path

import pytest
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from voxprint_lucent.tools_text import label
from voxprint_lucent.tools_window import ToolsWindow, sample_pixel


def test_sample_pixel_uses_the_centered_pixmap() -> None:
    assert sample_pixel((100, 100), (40, 20), (80, 40), (50.0, 50.0)) == (40, 20)
    assert sample_pixel((100, 100), (40, 20), (80, 40), (0.0, 0.0)) is None


def test_window_lists_facts_and_uses_the_palette(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setenv("LUCENT_LANG", "en")
    _ = QApplication.instance() or QApplication([])
    path = tmp_path / "dot.png"
    image = QImage(5, 2, QImage.Format.Format_RGB32)
    image.fill(0xFF112233)
    assert image.save(str(path), "PNG")
    window = ToolsWindow(path)
    text = window.info_text()
    assert "5×2" in text
    assert "none" in text
    assert "#60a5fa" in window.styleSheet()
    assert label("export") == "Export frame"
    window.close()
