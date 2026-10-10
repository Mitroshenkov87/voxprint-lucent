"""Save a copy. Default format is AVIF. This module is not imported for a normal open."""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication

from voxprint_lucent.decode import DecodeError, decode_image
from voxprint_lucent.formats import (
    DEFAULT_OUTPUT_FORMAT,
    CodecMissing,
    OutputFormat,
    PngRejected,
    copy_destination,
    parse_output_format,
    write_raster,
)


def export_copy(args) -> int:  # type: ignore[no-untyped-def]
    path: Path | None = args.image
    if path is None:
        print("usage: voxprint-lucent IMAGE --export [DEST]", file=sys.stderr)
        return 2
    if not path.is_file():
        print(f"No such image: {path}", file=sys.stderr)
        return 3
    try:
        fmt = DEFAULT_OUTPUT_FORMAT if args.format is None else parse_output_format(args.format)
    except (PngRejected, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
    app = QApplication.instance() or QApplication([])
    _ = app
    try:
        image = decode_image(path)
    except DecodeError as exc:
        print(str(exc), file=sys.stderr)
        return 3
    if args.export == "AUTO":
        dest = copy_destination(path, fmt)
    else:
        dest = Path(args.export)
        if dest.suffix.lower() == ".png":
            print("PNG is not an output format", file=sys.stderr)
            return 2
    try:
        written = write_qimage(image, dest, fmt, lossless=bool(args.lossless))
    except CodecMissing as exc:
        print(str(exc), file=sys.stderr)
        return 3
    print(written)
    return 0


def write_qimage(image: QImage, dest: Path, fmt: OutputFormat, *, lossless: bool) -> Path:
    rgba = image.convertToFormat(QImage.Format.Format_RGBA8888)
    width, height = rgba.width(), rgba.height()
    stride = rgba.bytesPerLine()
    raw = bytes(rgba.constBits())
    rows = np.frombuffer(raw, dtype=np.uint8).reshape((height, stride))
    pixels = np.ascontiguousarray(rows[:, : width * 4].reshape((height, width, 4)))
    return write_raster(pixels, dest, fmt, lossless=lossless)
