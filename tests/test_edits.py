"""Tools edits: orientation, export policy, crop, and trash."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest
from jpeg_sample import sample_jpeg
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from voxprint_lucent.edits import (
    color_text,
    crop_to_avif,
    export_frame,
    next_orientation,
    patch_jpeg_orientation,
    reveal_command,
    rotate_or_flip,
    send_to_trash,
)
from voxprint_lucent.formats import PngRejected
from voxprint_lucent.info import parse_jpeg_exif


def test_orientation_cycles() -> None:
    value = 1
    for expected in (6, 3, 8, 1):
        value = next_orientation(value, "cw")
        assert value == expected
    assert next_orientation(next_orientation(1, "flip-h"), "flip-h") == 1
    assert next_orientation(next_orientation(6, "flip-v"), "flip-v") == 6


def test_patch_orientation_is_in_place(tmp_path: Path) -> None:
    path = tmp_path / "shot.jpg"
    path.write_bytes(sample_jpeg())
    assert patch_jpeg_orientation(path, 3)
    assert parse_jpeg_exif(path.read_bytes()).orientation == 3


def test_jpeg_rotate_rewrites_the_tag(tmp_path: Path) -> None:
    path = tmp_path / "shot.jpg"
    path.write_bytes(sample_jpeg())
    dest = rotate_or_flip(path, "cw")
    assert dest == path
    assert parse_jpeg_exif(path.read_bytes()).orientation == 3


def test_color_text() -> None:
    assert color_text(15, 0, 255) == ("#0f00ff", "rgb(15, 0, 255)")


def test_reveal_command_selects_the_file(tmp_path: Path) -> None:
    path = tmp_path / "a.png"
    command = reveal_command(path)
    if sys.platform == "win32":
        assert command[0] == "explorer"
        assert str(path) in command[1]
    else:
        assert command == ["xdg-open", str(path.parent)]


def test_export_frame_is_avif_not_png(tmp_path: Path) -> None:
    _ = QApplication.instance() or QApplication([])
    path = tmp_path / "dot.png"
    image = QImage(2, 2, QImage.Format.Format_RGB32)
    image.fill(0xFFFF0000)
    assert image.save(str(path), "PNG")
    with pytest.raises(PngRejected):
        export_frame(path, 0, "png")
    with pytest.raises(ValueError, match="AVIF"):
        export_frame(path, 0, "jpeg")
    dest = export_frame(path, 0, "webp")
    assert dest.suffix == ".webp"
    assert dest.is_file()
    assert dest.resolve() != path.resolve()


def test_crop_writes_avif_copy(tmp_path: Path) -> None:
    _ = QApplication.instance() or QApplication([])
    path = tmp_path / "dot.png"
    image = QImage(8, 8, QImage.Format.Format_RGB32)
    image.fill(0xFF00FF00)
    assert image.save(str(path), "PNG")
    dest = crop_to_avif(path, 1, 1, 4, 4)
    assert dest.suffix == ".avif"
    assert dest.is_file()
    assert path.is_file()


def test_trash_moves_the_file(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    path = tmp_path / "note.txt"
    path.write_text("hello", encoding="utf-8")
    if sys.platform == "win32":
        send_to_trash(path)
        assert not path.exists()
        return
    monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path / "share"))
    send_to_trash(path)
    assert not path.exists()
    trashed = list((tmp_path / "share" / "Trash" / "files").iterdir())
    assert len(trashed) == 1
    assert trashed[0].read_text(encoding="utf-8") == "hello"
    info = (tmp_path / "share" / "Trash" / "info" / f"{trashed[0].name}.trashinfo").read_text(
        encoding="utf-8"
    )
    assert str(path.resolve()) in info
    assert "DeletionDate=" in info
