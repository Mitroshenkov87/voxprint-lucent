"""Synthetic images for the Windows measurement zip.

The bytes are produced in the build. Nothing here is a photograph or a
copied sample file.
"""

from __future__ import annotations

from pathlib import Path

import imagecodecs
import numpy as np

TWELVE = (4000, 3000)
TWENTY_FOUR = (6000, 4000)
OTHER = (640, 360)

# Display P3 primaries are a published chromaticity, not a copied profile.
_WIDE_PRIMARIES = (0.680, 0.320, 0.265, 0.690, 0.150, 0.060)
_D65 = (0.3127, 0.3290)


def write_hard_images(
    root: Path,
    *,
    twelve: tuple[int, int] = TWELVE,
    twenty_four: tuple[int, int] = TWENTY_FOUR,
    other: tuple[int, int] = OTHER,
) -> dict[str, Path]:
    """Write the VM set under ``root`` and return the paths."""
    root.mkdir(parents=True, exist_ok=True)
    large = _gradient(*twelve)
    huge = _gradient(*twenty_four)
    small = _gradient(*other)
    paths = {
        "avif-420-12mp.avif": root / "avif-420-12mp.avif",
        "avif-444-12mp.avif": root / "avif-444-12mp.avif",
        "avif-pq-10bit.avif": root / "avif-pq-10bit.avif",
        "jxl-lossy.jxl": root / "jxl-lossy.jxl",
        "jxl-lossless.jxl": root / "jxl-lossless.jxl",
        "jxl-progressive.jxl": root / "jxl-progressive.jxl",
        "jxl-pq.jxl": root / "jxl-pq.jxl",
        "animated.webp": root / "animated.webp",
        "orient6-24mp.jpg": root / "orient6-24mp.jpg",
        "wide-gamut.jpg": root / "wide-gamut.jpg",
    }
    paths["avif-420-12mp.avif"].write_bytes(_avif(large, "YUV420"))
    paths["avif-444-12mp.avif"].write_bytes(_avif(large, "YUV444"))
    paths["avif-pq-10bit.avif"].write_bytes(_avif_pq(small))
    lossy = _jxl(small, lossless=False)
    paths["jxl-lossy.jxl"].write_bytes(lossy)
    paths["jxl-lossless.jxl"].write_bytes(_jxl(small, lossless=True))
    paths["jxl-progressive.jxl"].write_bytes(_progressive_jxl(lossy))
    paths["jxl-pq.jxl"].write_bytes(_jxl_pq(small))
    paths["animated.webp"].write_bytes(_animated_webp(small))
    paths["orient6-24mp.jpg"].write_bytes(_with_exif_orientation(_jpeg(huge), 6))
    paths["wide-gamut.jpg"].write_bytes(_with_icc(_jpeg(small), _wide_profile()))
    return paths


def _gradient(width: int, height: int) -> np.ndarray:
    image = np.empty((height, width, 3), np.uint8)
    x = np.linspace(0, 255, width, dtype=np.float32)
    y = np.linspace(0, 255, height, dtype=np.float32)
    image[:, :, 0] = x[None, :]
    image[:, :, 1] = y[:, None]
    image[:, :, 2] = 96
    image[::32, :, 2] = 220
    image[:, ::32, 0] = 20
    return image


def _avif(image: np.ndarray, pixelformat: str) -> bytes:
    return bytes(imagecodecs.avif_encode(image, level=60, speed=8, pixelformat=pixelformat))


def _avif_pq(image: np.ndarray) -> bytes:
    sample = image.astype(np.uint16) << 2
    return bytes(
        imagecodecs.avif_encode(
            sample,
            speed=8,
            bitspersample=10,
            pixelformat="YUV444",
            primaries="BT2020",
            transfer="PQ",
        )
    )


def _jxl(image: np.ndarray, *, lossless: bool) -> bytes:
    if lossless:
        return bytes(imagecodecs.jpegxl_encode(image, effort=3, lossless=True))
    return bytes(imagecodecs.jpegxl_encode(image, effort=3, distance=1.0))


def _jxl_pq(image: np.ndarray) -> bytes:
    sample = image.astype(np.uint16) << 2
    return bytes(
        imagecodecs.jpegxl_encode(
            sample,
            effort=3,
            distance=1.0,
            bitspersample=10,
            primaries="BT2100",
            transfer="PQ",
        )
    )


def _progressive_jxl(codestream: bytes) -> bytes:
    """Wrap a codestream in ``jxlp`` boxes, the progressive JPEG XL container."""
    parts = 3
    chunks: list[bytes] = []
    start = 0
    count = len(codestream)
    for index in range(parts):
        end = count if index == parts - 1 else count * (index + 1) // parts
        last = 1 if index == parts - 1 else 0
        header = ((last << 31) | index).to_bytes(4, "big")
        chunks.append(_box(b"jxlp", header + codestream[start:end]))
        start = end
    signature = _box(b"JXL ", b"\r\n\x87\n")
    brand = _box(b"ftyp", b"jxl \x00\x00\x00\x00jxl ")
    return signature + brand + b"".join(chunks)


def _box(kind: bytes, payload: bytes) -> bytes:
    return (8 + len(payload)).to_bytes(4, "big") + kind + payload


def _animated_webp(image: np.ndarray) -> bytes:
    other = image[:, ::-1].copy()
    frames = np.stack((image, other), axis=0)
    return bytes(imagecodecs.webp_encode(frames, level=70, delay=100))


def _jpeg(image: np.ndarray) -> bytes:
    return bytes(imagecodecs.jpeg_encode(image, level=90))


def _wide_profile() -> bytes:
    return bytes(
        imagecodecs.cms_profile(
            "rgb",
            whitepoint=_D65,
            primaries=_WIDE_PRIMARIES,
            gamma=2.2,
        )
    )


def _with_exif_orientation(jpeg: bytes, orientation: int) -> bytes:
    if jpeg[:2] != b"\xff\xd8":
        raise ValueError("JPEG data does not start with SOI")
    count = (1).to_bytes(2, "little")
    entry = (
        (0x0112).to_bytes(2, "little")
        + (3).to_bytes(2, "little")
        + (1).to_bytes(4, "little")
        + int(orientation).to_bytes(2, "little")
        + b"\x00\x00"
    )
    tiff = b"II" + (42).to_bytes(2, "little") + (8).to_bytes(4, "little")
    tiff += count + entry + b"\x00\x00\x00\x00"
    return jpeg[:2] + _marker(0xE1, b"Exif\x00\x00" + tiff) + jpeg[2:]


def _with_icc(jpeg: bytes, profile: bytes) -> bytes:
    if jpeg[:2] != b"\xff\xd8":
        raise ValueError("JPEG data does not start with SOI")
    payload = b"ICC_PROFILE\x00" + bytes((1, 1)) + profile
    return jpeg[:2] + _marker(0xE2, payload) + jpeg[2:]


def _marker(marker: int, payload: bytes) -> bytes:
    length = len(payload) + 2
    if length > 65535:
        raise ValueError("JPEG marker is too large")
    return bytes((0xFF, marker)) + length.to_bytes(2, "big") + payload
