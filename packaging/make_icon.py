"""Draw Lucent's icon master and pack the suite sizes.

The mark is original: a dark rounded tile and a clear frame with a lens.
It follows the suite icon style and does not reuse another app's artwork.
voxprint-common v0.6.4 has no ``lucent`` identity colour, so the glyph uses
``#5ce1ff``, which is not one of the colours already in ``tokens.json``.
"""

from __future__ import annotations

import struct
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SHARE = ROOT / "share"
MASTER = 1024
SIZES = (16, 24, 32, 48, 64, 128, 256)
TILE = "#1b1b23"
LIFT = "#2c2c38"
GLYPH = (92, 225, 255, 255)
HIGHLIGHT = (184, 243, 255, 255)


def _blend(top: str, bottom: str, t: float) -> tuple[int, int, int, int]:
    def channel(hex_colour: str) -> tuple[int, int, int]:
        text = hex_colour.removeprefix("#")
        return int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16)

    a = channel(top)
    b = channel(bottom)
    mixed = tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))
    return mixed[0], mixed[1], mixed[2], 255


def _tile(size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    margin = round(size * 0.04)
    side = size - 2 * margin
    radius = round(side * 0.18)
    gradient = Image.new("RGBA", (side, side), TILE)
    draw = ImageDraw.Draw(gradient)
    for y in range(side):
        draw.line([(0, y), (side, y)], fill=_blend(TILE, LIFT, y / max(1, side - 1)))
    mask = Image.new("L", (side, side), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, side - 1, side - 1), radius=radius, fill=255)
    image.paste(gradient, (margin, margin), mask)
    return image


def _glyph(image: Image.Image) -> None:
    size = image.width
    margin = round(size * 0.04)
    side = size - 2 * margin
    glyph = round(side * 0.60)
    left = margin + (side - glyph) // 2
    top = margin + (side - glyph) // 2
    stroke = max(2, round(glyph * 0.08))
    radius = round(glyph * 0.16)
    draw = ImageDraw.Draw(image)
    box = (left, top, left + glyph - 1, top + glyph - 1)
    draw.rounded_rectangle(box, radius=radius, outline=GLYPH, width=stroke)
    highlight = (left + stroke, top + stroke, left + glyph - 1 - stroke, top + glyph // 3)
    draw.arc(highlight, 200, 340, fill=HIGHLIGHT, width=max(1, stroke // 3))
    lens = round(glyph * 0.34)
    cx = left + glyph // 2
    cy = top + glyph // 2
    draw.ellipse((cx - lens // 2, cy - lens // 2, cx + lens // 2, cy + lens // 2), fill=GLYPH)


def master() -> Image.Image:
    """Return the 1024 px master."""
    image = _tile(MASTER)
    _glyph(image)
    return image


def _png(image: Image.Image) -> bytes:
    from io import BytesIO

    buffer = BytesIO()
    image.save(buffer, format="PNG")
    return buffer.getvalue()


def pack_ico(frames: list[bytes]) -> bytes:
    """Pack square PNG frames into one ``.ico``, smallest first."""
    out = bytearray(struct.pack("<HHH", 0, 1, len(frames)))
    offset = 6 + 16 * len(frames)
    sized: list[tuple[int, bytes]] = []
    for data in frames:
        width, height = struct.unpack(">II", data[16:24])
        sized.append((width, data))
    sized.sort()
    body = bytearray()
    for width, data in sized:
        dim = width % 256
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        body += data
    return bytes(out + body)


def main() -> None:
    """Write the master, the size frames, the ``.ico``, and the window PNG."""
    ASSETS.mkdir(parents=True, exist_ok=True)
    art = master()
    art.save(ASSETS / "voxprint-lucent-1024.png")
    frames: list[bytes] = []
    for size in SIZES:
        frame = art.resize((size, size), Image.Resampling.LANCZOS)
        path = ASSETS / f"icon-{size}.png"
        frame.save(path)
        frames.append(path.read_bytes())
    (ASSETS / "voxprint-lucent.ico").write_bytes(pack_ico(frames))
    art.resize((256, 256), Image.Resampling.LANCZOS).save(SHARE / "voxprint-lucent.png")
    print(f"wrote {ASSETS / 'voxprint-lucent.ico'}")


if __name__ == "__main__":
    main()
