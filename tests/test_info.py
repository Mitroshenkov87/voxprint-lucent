"""EXIF summary and info-panel facts. No GPL metadata library."""

from __future__ import annotations

from pathlib import Path

from jpeg_sample import sample_jpeg
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from voxprint_lucent.info import facts_for, orientation_value_span, parse_jpeg_exif


def test_jpeg_exif_summary() -> None:
    blob = sample_jpeg()
    summary = parse_jpeg_exif(blob)
    assert summary.orientation == 6
    assert summary.make == "HTC"
    assert summary.model == "Phone"
    assert summary.taken == "2020:01:02 03:04:05"
    assert summary.gps == "37.80000, -122.40000"
    assert "HTC" in summary.line
    offset, endian = orientation_value_span(blob) or (0, "")
    assert endian == "little"
    assert int.from_bytes(blob[offset : offset + 2], "little") == 6


def test_png_facts_have_no_exif(tmp_path: Path) -> None:
    _ = QApplication.instance() or QApplication([])
    path = tmp_path / "dot.png"
    image = QImage(4, 3, QImage.Format.Format_RGB32)
    image.fill(0xFF336699)
    assert image.save(str(path), "PNG")
    facts = facts_for(path)
    assert facts.width == 4
    assert facts.height == 3
    assert facts.bit_depth == 32
    assert facts.gps == "none"
    assert facts.exif_line == "none"
    assert facts.frame_count == 1
