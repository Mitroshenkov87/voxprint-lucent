"""Window shots keep the picture below the chips and cover the window edges."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHOTS = ROOT / "docs" / "img" / "shell"
# Same bands as viewerBands() in cpp/src/shell_logic.cpp.
PAGE_TOP = 12
BAR_HEIGHT = 40
BAR_GAP = 10
ROW_GAP = 8
CHIP = 44
PAGE_BOTTOM = 12
STATUS = 36
HISTORY = 104
ROOT_COLOR = (23, 23, 28)
WINDOW = (1200, 900)


def _viewer_picture_top(window_height: int, history: bool) -> int:
    chip_top = PAGE_TOP + BAR_HEIGHT + BAR_GAP
    chip_height = CHIP + ROW_GAP + CHIP
    return chip_top + chip_height + ROW_GAP


def _paeth(left: int, up: int, upper_left: int) -> int:
    estimate = left + up - upper_left
    candidates = (
        (abs(estimate - left), left),
        (abs(estimate - up), up),
        (abs(estimate - upper_left), upper_left),
    )
    nearest = min(candidates)
    return nearest[1]


def _read_png(path: Path) -> tuple[int, int, int, list[bytearray]]:
    data = path.read_bytes()
    assert data.startswith(b"\x89PNG\r\n\x1a\n"), path.name
    pos = 8
    width = height = 0
    bit_depth = color_type = interlace = 0
    idat = bytearray()
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        kind = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            unpacked = struct.unpack(">IIBBBBB", chunk)
            width = unpacked[0]
            height = unpacked[1]
            bit_depth = unpacked[2]
            color_type = unpacked[3]
            interlace = unpacked[6]
        elif kind == b"IDAT":
            idat.extend(chunk)
        elif kind == b"IEND":
            break
    assert bit_depth == 8 and color_type in {2, 6} and interlace == 0, path.name
    channels = 3 if color_type == 2 else 4
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    rows: list[bytearray] = []
    cursor = 0
    previous = bytearray(stride)
    for _y in range(height):
        filter_type = raw[cursor]
        cursor += 1
        row = bytearray(raw[cursor : cursor + stride])
        cursor += stride
        if filter_type == 1:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + left) & 255
        elif filter_type == 2:
            for x in range(stride):
                row[x] = (row[x] + previous[x]) & 255
        elif filter_type == 3:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                row[x] = (row[x] + ((left + previous[x]) // 2)) & 255
        elif filter_type == 4:
            for x in range(stride):
                left = row[x - channels] if x >= channels else 0
                up = previous[x]
                upper_left = previous[x - channels] if x >= channels else 0
                row[x] = (row[x] + _paeth(left, up, upper_left)) & 255
        elif filter_type != 0:
            raise AssertionError(f"{path.name} uses PNG filter {filter_type}")
        previous = row
        rows.append(row)
    return width, height, channels, rows


def _pixel(rows: list[bytearray], channels: int, x: int, y: int) -> tuple[int, int, int]:
    index = x * channels
    row = rows[y]
    return (row[index], row[index + 1], row[index + 2])


def test_picture_band_stays_below_the_chips() -> None:
    picture_top = _viewer_picture_top(900, True)
    chip_bottom = PAGE_TOP + BAR_HEIGHT + BAR_GAP + CHIP + ROW_GAP + CHIP
    assert picture_top >= chip_bottom
    assert picture_top == 166
    short_bottom = 640 - PAGE_BOTTOM - HISTORY - STATUS
    assert short_bottom > _viewer_picture_top(640, True)


def test_window_shots_cover_their_edges_and_leave_the_chip_margin_clear() -> None:
    picture_top = _viewer_picture_top(WINDOW[1], True)
    for name in ("simple-en", "simple-ru", "expert-en", "expert-ru", "overflow-en", "overflow-ru"):
        width, height, channels, rows = _read_png(SHOTS / f"{name}.png")
        assert (width, height) == WINDOW, name
        for x in range(width):
            for y in (0, height - 1):
                assert _pixel(rows, channels, x, y) != (0, 0, 0), name
        for y in range(height):
            for x in (0, width - 1):
                assert _pixel(rows, channels, x, y) != (0, 0, 0), name
        for y in range(picture_top):
            assert _pixel(rows, channels, 4, y) == ROOT_COLOR, name


def test_enlarge_confirm_shots_exist() -> None:
    for name in ("enlarge-confirm-en.png", "enlarge-confirm-ru.png"):
        path = SHOTS / name
        width, height, _channels, _rows = _read_png(path)
        assert width >= 320 and height >= 80
