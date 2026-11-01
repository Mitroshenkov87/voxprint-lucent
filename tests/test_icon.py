"""The app icon matches the suite frame list."""

from __future__ import annotations

import struct
from pathlib import Path

MASTER = Path("assets/voxprint-lucent-1024.png")
ICON = Path("assets/voxprint-lucent.ico")
SIZES = (16, 24, 32, 48, 64, 128, 256)


def test_master_is_1024() -> None:
    data = MASTER.read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n")
    width, height = struct.unpack(">II", data[16:24])
    assert (width, height) == (1024, 1024)


def test_ico_has_every_suite_size() -> None:
    data = ICON.read_bytes()
    _reserved, kind, count = struct.unpack("<HHH", data[:6])
    assert kind == 1
    found = []
    for index in range(count):
        width = data[6 + 16 * index]
        height = data[7 + 16 * index]
        found.append(width or 256)
        assert width == height
    assert tuple(sorted(found)) == SIZES
    for size in SIZES:
        assert (Path("assets") / f"icon-{size}.png").is_file()
    assert Path("share/voxprint-lucent.png").is_file()
