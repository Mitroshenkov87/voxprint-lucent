"""Read-only image facts for the tools info panel.

EXIF is parsed here. exiv2 is GPL and is not used.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PySide6.QtGui import QImageReader

from voxprint_lucent.decode import DecodeError


@dataclass(frozen=True)
class ExifSummary:
    orientation: int | None
    make: str
    model: str
    taken: str
    gps: str

    @property
    def line(self) -> str:
        parts = [part for part in (self.make, self.model, self.taken) if part]
        if self.orientation is not None:
            parts.append(f"orientation {self.orientation}")
        if not parts:
            return "none"
        return ", ".join(parts)


@dataclass(frozen=True)
class ImageFacts:
    path: str
    width: int
    height: int
    format_name: str
    bit_depth: int
    color_profile: str
    exif_line: str
    gps: str
    frame_count: int
    frame_index: int


def parse_jpeg_exif(data: bytes) -> ExifSummary:
    """EXIF summary from a JPEG. Missing APP1 yields an empty summary."""
    empty = ExifSummary(None, "", "", "", "none")
    payload = _first_exif_payload(data)
    if payload is None:
        return empty
    return _parse_exif_tiff(payload) or empty


def orientation_value_span(data: bytes) -> tuple[int, str] | None:
    """File offset and endian of the orientation short, when the tag is inline."""
    start = _exif_tiff_offset(data)
    if start is None:
        return None
    blob = data[start:]
    if len(blob) < 8:
        return None
    endian = "little" if blob[:2] == b"II" else "big" if blob[:2] == b"MM" else ""
    if not endian:
        return None
    if _u16(blob, 2, endian) != 42:
        return None
    ifd = _u32(blob, 4, endian)
    if ifd + 2 > len(blob):
        return None
    count = _u16(blob, ifd, endian)
    for index in range(count):
        entry = ifd + 2 + index * 12
        if entry + 12 > len(blob):
            return None
        tag = _u16(blob, entry, endian)
        kind = _u16(blob, entry + 2, endian)
        number = _u32(blob, entry + 4, endian)
        if tag == 0x0112 and kind == 3 and number == 1:
            return start + entry + 8, endian
    return None


def facts_for(path: Path, frame_index: int = 0) -> ImageFacts:
    reader = QImageReader(str(path))
    reader.setAutoTransform(True)
    frame_count = max(reader.imageCount(), 1)
    if frame_index > 0 and not reader.jumpToImage(frame_index):
        raise DecodeError(path, f"No frame {frame_index} in {path.name}")
    image = reader.read()
    if image.isNull():
        raise DecodeError(path, f"Could not decode {path.name}")
    profile = "none"
    space = image.colorSpace()
    if space.isValid():
        description = space.description().strip()
        profile = description or "sRGB"
    exif = (
        parse_jpeg_exif(path.read_bytes())
        if path.suffix.lower() in {".jpg", ".jpeg", ".jpe"}
        else ExifSummary(None, "", "", "", "none")
    )
    format_name = bytes(reader.format()).decode("ascii", "replace")
    if not format_name:
        format_name = path.suffix.lower().lstrip(".")
    return ImageFacts(
        path=str(path),
        width=image.width(),
        height=image.height(),
        format_name=format_name,
        bit_depth=image.depth(),
        color_profile=profile,
        exif_line=exif.line,
        gps=exif.gps,
        frame_count=frame_count,
        frame_index=frame_index,
    )


def _first_exif_payload(data: bytes) -> bytes | None:
    for payload in _app1_payloads(data):
        if payload.startswith(b"Exif\x00\x00") and len(payload) > 6:
            return payload[6:]
    return None


def _exif_tiff_offset(data: bytes) -> int | None:
    offset = 2
    while offset + 4 <= len(data) and data[offset] == 0xFF:
        marker = data[offset + 1]
        if marker == 0xDA:
            return None
        if marker in {0xD8, 0xD9}:
            offset += 2
            continue
        length = int.from_bytes(data[offset + 2 : offset + 4], "big")
        if length < 2 or offset + 2 + length > len(data):
            return None
        if marker == 0xE1:
            payload = data[offset + 4 : offset + 2 + length]
            if payload.startswith(b"Exif\x00\x00"):
                return offset + 4 + 6
        offset += 2 + length
    return None


def _app1_payloads(data: bytes) -> list[bytes]:
    found: list[bytes] = []
    if len(data) < 2 or data[:2] != b"\xff\xd8":
        return found
    offset = 2
    while offset + 4 <= len(data) and data[offset] == 0xFF:
        marker = data[offset + 1]
        if marker == 0xDA:
            break
        if marker in {0xD8, 0xD9}:
            offset += 2
            continue
        length = int.from_bytes(data[offset + 2 : offset + 4], "big")
        if length < 2 or offset + 2 + length > len(data):
            break
        if marker == 0xE1:
            found.append(data[offset + 4 : offset + 2 + length])
        offset += 2 + length
    return found


def _parse_exif_tiff(blob: bytes) -> ExifSummary | None:
    if len(blob) < 8:
        return None
    endian = "little" if blob[:2] == b"II" else "big" if blob[:2] == b"MM" else ""
    if not endian or _u16(blob, 2, endian) != 42:
        return None
    ifd = _u32(blob, 4, endian)
    fields = _ifd(blob, ifd, endian)
    if fields is None:
        return None
    gps_offset = fields.get(0x8825)
    gps = "none"
    if isinstance(gps_offset, int):
        gps_fields = _ifd(blob, gps_offset, endian)
        if gps_fields is not None:
            gps = _gps_line(blob, gps_fields, endian)
    make = fields.get(0x010F, "")
    model = fields.get(0x0110, "")
    taken = fields.get(0x0132, "")
    orientation = fields.get(0x0112)
    return ExifSummary(
        orientation if isinstance(orientation, int) else None,
        make if isinstance(make, str) else "",
        model if isinstance(model, str) else "",
        taken if isinstance(taken, str) else "",
        gps,
    )


def _ifd(blob: bytes, offset: int, endian: str) -> dict[int, str | int] | None:
    if offset < 0 or offset + 2 > len(blob):
        return None
    count = _u16(blob, offset, endian)
    fields: dict[int, str | int] = {}
    for index in range(count):
        entry = offset + 2 + index * 12
        if entry + 12 > len(blob):
            return None
        tag = _u16(blob, entry, endian)
        kind = _u16(blob, entry + 2, endian)
        number = _u32(blob, entry + 4, endian)
        value = _field_value(blob, entry + 8, kind, number, endian)
        if value is not None:
            fields[tag] = value
    return fields


def _field_value(
    blob: bytes, value_at: int, kind: int, number: int, endian: str
) -> str | int | None:
    widths = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8}
    width = widths.get(kind)
    if width is None or number <= 0:
        return None
    size = width * number
    if size <= 4:
        raw = blob[value_at : value_at + 4]
        location = 0
    else:
        pointed = _u32(blob, value_at, endian)
        if pointed + size > len(blob):
            return None
        raw = blob[pointed : pointed + size]
        location = 0
    if kind == 2:
        text = raw[:number].split(b"\x00", 1)[0]
        return text.decode("ascii", "replace").strip()
    if kind == 3 and number == 1:
        return int.from_bytes(raw[location : location + 2], endian)
    if kind == 4 and number == 1:
        return int.from_bytes(raw[location : location + 4], endian)
    if kind == 5 and number == 3 and size > 4:
        return _u32(blob, value_at, endian)
    return None


def _gps_line(blob: bytes, fields: dict[int, str | int], endian: str) -> str:
    lat = _coord(blob, fields.get(0x0002), fields.get(0x0001), endian)
    lon = _coord(blob, fields.get(0x0004), fields.get(0x0003), endian)
    if lat is None or lon is None:
        return "none"
    return f"{lat:.5f}, {lon:.5f}"


def _coord(
    blob: bytes, value: str | int | None, ref: str | int | None, endian: str
) -> float | None:
    if not isinstance(value, int) or not isinstance(ref, str):
        return None
    if value < 0 or value + 24 > len(blob):
        return None
    parts: list[float] = []
    for index in range(3):
        base = value + index * 8
        numerator = _u32(blob, base, endian)
        denominator = _u32(blob, base + 4, endian)
        if denominator == 0:
            return None
        parts.append(numerator / denominator)
    signed = parts[0] + parts[1] / 60 + parts[2] / 3600
    if ref in {"S", "W"}:
        signed = -signed
    return signed


def _u16(blob: bytes, offset: int, endian: str) -> int:
    return int.from_bytes(blob[offset : offset + 2], endian)


def _u32(blob: bytes, offset: int, endian: str) -> int:
    return int.from_bytes(blob[offset : offset + 4], endian)
