"""Edits that stay in the tools process: frames, rotate, crop, trash, reveal."""

from __future__ import annotations

import os
import sys
from datetime import UTC, datetime
from pathlib import Path

from PySide6.QtGui import QImage, QImageReader, QTransform

from voxprint_lucent.decode import DecodeError, decode_image
from voxprint_lucent.exporting import write_qimage
from voxprint_lucent.formats import (
    MODERN_OUTPUT_FORMATS,
    OutputFormat,
    copy_destination,
    parse_output_format,
)
from voxprint_lucent.info import orientation_value_span, parse_jpeg_exif

# EXIF orientation after a clockwise quarter-turn, a horizontal flip, or a vertical flip.
ROTATE_CW = {1: 6, 2: 7, 3: 8, 4: 5, 5: 2, 6: 3, 7: 4, 8: 1}
FLIP_H = {1: 2, 2: 1, 3: 4, 4: 3, 5: 6, 6: 5, 7: 8, 8: 7}
FLIP_V = {1: 4, 2: 3, 3: 2, 4: 1, 5: 8, 6: 7, 7: 6, 8: 5}


def next_orientation(current: int, operation: str) -> int:
    table = {"cw": ROTATE_CW, "flip-h": FLIP_H, "flip-v": FLIP_V}[operation]
    return table[current if current in table else 1]


def color_text(red: int, green: int, blue: int) -> tuple[str, str]:
    return f"#{red:02x}{green:02x}{blue:02x}", f"rgb({red}, {green}, {blue})"


def reveal_command(path: Path) -> list[str]:
    if sys.platform == "win32":
        return ["explorer", f"/select,{path}"]
    return ["xdg-open", str(path.parent)]


def send_to_trash(path: Path) -> None:
    """Move a file to the system trash. The source path no longer exists."""
    if sys.platform == "win32":
        _trash_windows(path)
        return
    _trash_freedesktop(path)


def patch_jpeg_orientation(path: Path, orientation: int) -> bool:
    """Rewrite the orientation short in place. False when the tag is absent."""
    data = path.read_bytes()
    found = orientation_value_span(data)
    if found is None:
        return False
    offset, endian = found
    patched = bytearray(data)
    patched[offset : offset + 2] = int(orientation).to_bytes(2, endian)
    path.write_bytes(bytes(patched))
    return True


def rotate_or_flip(path: Path, operation: str) -> Path:
    """Lossless JPEG tag update when the tag exists. Otherwise an AVIF copy."""
    if operation not in {"cw", "flip-h", "flip-v"}:
        raise ValueError(f"Unknown transform: {operation}")
    suffix = path.suffix.lower()
    if suffix in {".jpg", ".jpeg", ".jpe"}:
        current = parse_jpeg_exif(path.read_bytes()).orientation or 1
        updated = next_orientation(current, operation)
        if patch_jpeg_orientation(path, updated):
            return path
    image = decode_image(path)
    if operation == "cw":
        turned = image.transformed(QTransform().rotate(90))
    elif operation == "flip-h":
        turned = image.mirrored(True, False)
    else:
        turned = image.mirrored(False, True)
    dest = copy_destination(path, OutputFormat.AVIF)
    return write_qimage(turned, dest, OutputFormat.AVIF, lossless=False)


def crop_to_avif(path: Path, x: int, y: int, width: int, height: int, frame_index: int = 0) -> Path:
    image = read_frame(path, frame_index)
    cropped = image.copy(x, y, width, height)
    dest = copy_destination(path, OutputFormat.AVIF)
    return write_qimage(cropped, dest, OutputFormat.AVIF, lossless=False)


def export_frame(path: Path, frame_index: int, format_name: str) -> Path:
    fmt = parse_output_format(format_name)
    if fmt not in MODERN_OUTPUT_FORMATS:
        raise ValueError("Frame export is AVIF, JPEG XL, or WebP")
    image = read_frame(path, frame_index)
    dest = copy_destination(path, fmt)
    return write_qimage(image, dest, fmt, lossless=fmt is not OutputFormat.AVIF)


def read_frame(path: Path, frame_index: int) -> QImage:
    reader = QImageReader(str(path))
    reader.setAutoTransform(True)
    if frame_index > 0 and not reader.jumpToImage(frame_index):
        raise DecodeError(path, f"No frame {frame_index} in {path.name}")
    image = reader.read()
    if image.isNull():
        raise DecodeError(path, f"Could not decode {path.name}")
    return image


def _trash_freedesktop(path: Path) -> None:
    data_home = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local" / "share")))
    files = data_home / "Trash" / "files"
    info = data_home / "Trash" / "info"
    files.mkdir(parents=True, exist_ok=True)
    info.mkdir(parents=True, exist_ok=True)
    target = files / path.name
    number = 2
    while target.exists():
        target = files / f"{path.stem}-{number}{path.suffix}"
        number += 1
    path.rename(target)
    stamp = datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%S")
    (info / f"{target.name}.trashinfo").write_text(
        "[Trash Info]\n"
        f"Path={path.resolve()}\n"
        f"DeletionDate={stamp}\n",
        encoding="utf-8",
    )


def _trash_windows(path: Path) -> None:
    # ctypes.wintypes is Windows-only. A top-level import fails on Linux.
    import ctypes
    from ctypes import wintypes

    class SHFILEOPSTRUCTW(ctypes.Structure):
        _fields_ = [
            ("hwnd", wintypes.HWND),
            ("wFunc", ctypes.c_uint),
            ("pFrom", ctypes.c_wchar_p),
            ("pTo", ctypes.c_wchar_p),
            ("fFlags", ctypes.c_uint16),
            ("fAnyOperationsAborted", wintypes.BOOL),
            ("hNameMappings", ctypes.c_void_p),
            ("lpszProgressTitle", ctypes.c_wchar_p),
        ]

    fo_delete = 3
    fof_allowundo = 0x0040
    fof_noconfirmation = 0x0010
    fof_silent = 0x0004
    fof_noerrorui = 0x0400
    op = SHFILEOPSTRUCTW()
    op.wFunc = fo_delete
    op.pFrom = str(path.resolve()) + "\0\0"
    op.fFlags = fof_allowundo | fof_noconfirmation | fof_silent | fof_noerrorui
    result = ctypes.windll.shell32.SHFileOperationW(ctypes.byref(op))
    if result != 0 or op.fAnyOperationsAborted:
        raise OSError(f"Could not move {path.name} to the Recycle Bin ({result})")
