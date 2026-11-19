"""Output format policy: WebP default, JPEG XL and AVIF options, no PNG."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import pytest

from voxprint_lucent.formats import (
    DEFAULT_OUTPUT_FORMAT,
    LEGACY_OUTPUT_FORMATS,
    MODERN_OUTPUT_FORMATS,
    CodecMissing,
    OutputFormat,
    PngRejected,
    choose_output_format,
    copy_destination,
    encode_raster,
    parse_output_format,
    write_raster,
)


def test_default_is_webp() -> None:
    assert DEFAULT_OUTPUT_FORMAT is OutputFormat.WEBP
    assert OutputFormat.AVIF in MODERN_OUTPUT_FORMATS
    assert OutputFormat.JXL in MODERN_OUTPUT_FORMATS
    assert OutputFormat.WEBP in MODERN_OUTPUT_FORMATS
    assert OutputFormat.JPEG in LEGACY_OUTPUT_FORMATS
    assert OutputFormat.HEIC in LEGACY_OUTPUT_FORMATS


def test_png_is_rejected() -> None:
    with pytest.raises(PngRejected):
        parse_output_format("png")
    with pytest.raises(PngRejected):
        parse_output_format(".PNG")


def test_aliases_and_unknown() -> None:
    assert parse_output_format("jpg") is OutputFormat.JPEG
    assert parse_output_format("jxl") is OutputFormat.JXL
    with pytest.raises(ValueError):
        parse_output_format("tiff")


def test_copy_destination_does_not_overwrite_source(tmp_path: Path) -> None:
    source = tmp_path / "photo.jpeg"
    source.write_bytes(b"jpeg")
    dest = copy_destination(source)
    assert dest.name == "photo.webp"
    assert dest != source
    dest.write_bytes(b"webp")
    again = copy_destination(source)
    assert again.name == "photo-2.webp"


def test_copy_of_webp_uses_a_distinct_name(tmp_path: Path) -> None:
    source = tmp_path / "photo.webp"
    source.write_bytes(b"webp")
    assert copy_destination(source).name == "photo.copy.webp"


def test_heic_needs_the_system_codec() -> None:
    pixels = np.zeros((2, 2, 3), dtype=np.uint8)
    with pytest.raises(CodecMissing):
        encode_raster(pixels, OutputFormat.HEIC)


def test_modern_round_trip_dimensions(tmp_path: Path) -> None:
    import imagecodecs

    pixels = np.zeros((4, 6, 3), dtype=np.uint8)
    pixels[:, :, 0] = 200
    pixels[0, 0] = (10, 20, 30)
    for fmt, decoder in (
        (OutputFormat.AVIF, imagecodecs.avif_decode),
        (OutputFormat.JXL, imagecodecs.jpegxl_decode),
        (OutputFormat.WEBP, imagecodecs.webp_decode),
    ):
        dest = tmp_path / f"out{fmt.value}"
        write_raster(pixels, dest, fmt)
        decoded = decoder(dest.read_bytes())
        assert decoded.shape[0] == 4
        assert decoded.shape[1] == 6


def test_default_write_is_webp(tmp_path: Path) -> None:
    pixels = np.full((2, 2, 3), 128, dtype=np.uint8)
    dest = tmp_path / "copy.webp"
    written = write_raster(pixels, dest)
    assert written.suffix == ".webp"
    assert dest.stat().st_size > 0


def test_oversized_webp_falls_back_to_jpeg() -> None:
    assert choose_output_format(16383, 100, OutputFormat.WEBP) is OutputFormat.WEBP
    assert choose_output_format(16384, 100, OutputFormat.WEBP) is OutputFormat.JPEG
    assert choose_output_format(100, 16384, OutputFormat.WEBP) is OutputFormat.JPEG
    assert choose_output_format(20000, 20000, OutputFormat.AVIF) is OutputFormat.AVIF
    assert choose_output_format(20000, 20000, OutputFormat.JPEG) is OutputFormat.JPEG
