"""A tiny JPEG with a hand-built EXIF block for the info and edit tests."""

from __future__ import annotations


def _u16(value: int) -> bytes:
    return value.to_bytes(2, "little")


def _u32(value: int) -> bytes:
    return value.to_bytes(4, "little")


def _entry(tag: int, kind: int, count: int, value: int) -> bytes:
    return _u16(tag) + _u16(kind) + _u32(count) + value.to_bytes(4, "little")


def sample_jpeg() -> bytes:
    """Little-endian EXIF: HTC / Phone, orientation 6, a GPS point at 37.8N 122.4W."""
    make = b"HTC\x00"
    model = b"Phone\x00"
    taken = b"2020:01:02 03:04:05\x00"
    ifd_count = 5
    data_at = 8 + 2 + ifd_count * 12 + 4
    model_at = data_at
    taken_at = model_at + len(model)
    gps_at = taken_at + len(taken)
    lat_at = gps_at + 2 + 4 * 12 + 4
    lon_at = lat_at + 24
    tiff = bytearray(b"II" + _u16(42) + _u32(8))
    tiff += _u16(ifd_count)
    tiff += _entry(0x010F, 2, len(make), int.from_bytes(make[:4], "little"))
    tiff += _entry(0x0110, 2, len(model), model_at)
    tiff += _entry(0x0112, 3, 1, 6)
    tiff += _entry(0x0132, 2, len(taken), taken_at)
    tiff += _entry(0x8825, 4, 1, gps_at)
    tiff += _u32(0)
    tiff += model + taken
    tiff += _u16(4)
    tiff += _entry(0x0001, 2, 2, int.from_bytes(b"N\x00\x00", "little"))
    tiff += _entry(0x0002, 5, 3, lat_at)
    tiff += _entry(0x0003, 2, 2, int.from_bytes(b"W\x00\x00", "little"))
    tiff += _entry(0x0004, 5, 3, lon_at)
    tiff += _u32(0)
    tiff += _u32(37) + _u32(1) + _u32(48) + _u32(1) + _u32(0) + _u32(1)
    tiff += _u32(122) + _u32(1) + _u32(24) + _u32(1) + _u32(0) + _u32(1)
    payload = b"Exif\x00\x00" + bytes(tiff)
    length = len(payload) + 2
    return b"\xff\xd8\xff\xe1" + length.to_bytes(2, "big") + payload + b"\xff\xd9"
