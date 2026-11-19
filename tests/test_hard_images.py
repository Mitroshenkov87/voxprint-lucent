"""The measurement-zip images are generated, and they have the properties named in the file."""

from __future__ import annotations

from pathlib import Path

import imagecodecs
import numpy as np

from bench.hard_images import OTHER, TWELVE, TWENTY_FOUR, write_hard_images
from voxprint_lucent.info import parse_jpeg_exif


def test_default_sizes_are_the_megapixel_targets() -> None:
    assert TWELVE == (4000, 3000)
    assert TWENTY_FOUR == (6000, 4000)
    assert OTHER[0] * OTHER[1] < 1_000_000


def test_hard_images_match_the_named_properties(tmp_path: Path) -> None:
    paths = write_hard_images(
        tmp_path,
        twelve=(32, 24),
        twenty_four=(48, 32),
        other=(16, 16),
    )
    yuv420 = imagecodecs.avif_decode(paths["avif-420-12mp.avif"].read_bytes())
    yuv444 = imagecodecs.avif_decode(paths["avif-444-12mp.avif"].read_bytes())
    assert yuv420.shape == (24, 32, 3)
    assert yuv444.shape == (24, 32, 3)
    assert _chroma_error("YUV420") > _chroma_error("YUV444")

    pq = imagecodecs.avif_decode(paths["avif-pq-10bit.avif"].read_bytes())
    assert pq.dtype == np.uint16
    assert int(pq.max()) > 255

    source = imagecodecs.jpegxl_decode(paths["jxl-lossless.jxl"].read_bytes())
    lossy = imagecodecs.jpegxl_decode(paths["jxl-lossy.jxl"].read_bytes())
    assert source.dtype == np.uint8
    assert not np.array_equal(source, lossy)
    progressive = paths["jxl-progressive.jxl"].read_bytes()
    assert b"jxlp" in progressive
    assert np.array_equal(imagecodecs.jpegxl_decode(progressive), lossy)
    hdr = imagecodecs.jpegxl_decode(paths["jxl-pq.jxl"].read_bytes())
    assert hdr.dtype == np.uint16

    webp = paths["animated.webp"].read_bytes()
    assert b"ANIM" in webp
    first = imagecodecs.webp_decode(webp, index=0)
    second = imagecodecs.webp_decode(webp, index=1)
    assert first.shape[2] == 3
    assert second.shape == first.shape

    orient = paths["orient6-24mp.jpg"].read_bytes()
    assert parse_jpeg_exif(orient).orientation == 6
    decoded = imagecodecs.jpeg_decode(orient)
    assert decoded.shape == (32, 48, 3)

    wide = paths["wide-gamut.jpg"].read_bytes()
    profile = _icc_profile(wide)
    info = imagecodecs.cms_info(profile)
    assert info["colorspace"] == "rgb"
    assert info["channels"] == 3


def _icc_profile(jpeg: bytes) -> bytes:
    offset = 2
    while offset + 4 <= len(jpeg) and jpeg[offset] == 0xFF:
        marker = jpeg[offset + 1]
        if marker == 0xDA:
            break
        length = int.from_bytes(jpeg[offset + 2 : offset + 4], "big")
        payload = jpeg[offset + 4 : offset + 2 + length]
        if marker == 0xE2 and payload.startswith(b"ICC_PROFILE\x00"):
            return payload[len(b"ICC_PROFILE\x00") + 2 :]
        offset += 2 + length
    raise AssertionError("JPEG has no ICC profile")


def _chroma_error(pixelformat: str) -> float:
    image = np.zeros((32, 32, 3), np.uint8)
    image[:, 0::2, 0] = 255
    image[:, 1::2, 2] = 255
    encoded = bytes(imagecodecs.avif_encode(image, level=60, speed=8, pixelformat=pixelformat))
    decoded = imagecodecs.avif_decode(encoded)
    red = decoded[:, 0::2, 0].astype(np.float32)
    return float(np.mean(np.abs(red - 255.0)))
