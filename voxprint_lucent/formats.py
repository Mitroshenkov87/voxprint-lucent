"""Image output policy. AVIF is the default. PNG is not an output format."""

from __future__ import annotations

from enum import Enum
from pathlib import Path
from typing import assert_never

import numpy as np
from numpy.typing import NDArray


class OutputFormat(Enum):
    AVIF = "avif"
    JXL = "jxl"
    WEBP = "webp"
    JPEG = "jpeg"
    HEIC = "heic"


DEFAULT_OUTPUT_FORMAT = OutputFormat.AVIF
MODERN_OUTPUT_FORMATS = (OutputFormat.AVIF, OutputFormat.JXL, OutputFormat.WEBP)
LEGACY_OUTPUT_FORMATS = (OutputFormat.JPEG, OutputFormat.HEIC)


class PngRejected(ValueError):
    """PNG is never an output format for this app."""


class CodecMissing(RuntimeError):
    """The system codec required for a legacy format is not installed."""


def parse_output_format(name: str) -> OutputFormat:
    key = name.lower().strip().lstrip(".")
    if key == "png":
        raise PngRejected("PNG is not an output format")
    if key == "jpg":
        return OutputFormat.JPEG
    try:
        return OutputFormat(key)
    except ValueError as exc:
        raise ValueError(f"Unsupported output format: {name}") from exc


def extension_for(fmt: OutputFormat) -> str:
    match fmt:
        case OutputFormat.AVIF:
            return ".avif"
        case OutputFormat.JXL:
            return ".jxl"
        case OutputFormat.WEBP:
            return ".webp"
        case OutputFormat.JPEG:
            return ".jpeg"
        case OutputFormat.HEIC:
            return ".heic"
        case _ as unreachable:
            assert_never(unreachable)


def copy_destination(source: Path, fmt: OutputFormat | None = None) -> Path:
    """Path for a saved copy. Never the source path itself."""
    chosen = DEFAULT_OUTPUT_FORMAT if fmt is None else fmt
    ext = extension_for(chosen)
    if source.suffix.lower() == ext:
        candidate = source.with_name(f"{source.stem}.copy{ext}")
    else:
        candidate = source.with_name(f"{source.stem}{ext}")
    return unique_path(candidate)


def unique_path(path: Path) -> Path:
    if not path.exists():
        return path
    stem, ext = path.stem, path.suffix
    number = 2
    while True:
        candidate = path.with_name(f"{stem}-{number}{ext}")
        if not candidate.exists():
            return candidate
        number += 1


def encode_raster(
    pixels: NDArray[np.uint8],
    fmt: OutputFormat,
    *,
    lossless: bool = False,
) -> bytes:
    """Encode an 8-bit raster. HEIC is refused here; the system codec owns it."""
    # Lazy: imagecodecs pulls NumPy into the save path only, never into first paint.
    import imagecodecs

    array = np.ascontiguousarray(pixels)
    match fmt:
        case OutputFormat.AVIF:
            encoded = imagecodecs.avif_encode(array)
        case OutputFormat.JXL:
            encoded = imagecodecs.jpegxl_encode(array, lossless=lossless)
        case OutputFormat.WEBP:
            encoded = imagecodecs.webp_encode(array, lossless=lossless)
        case OutputFormat.JPEG:
            if array.ndim == 3 and array.shape[2] == 4:
                array = np.ascontiguousarray(array[:, :, :3])
            encoded = imagecodecs.jpeg_encode(array)
        case OutputFormat.HEIC:
            raise CodecMissing(
                "HEIC output uses the system codec only. Install the platform HEIF codec."
            )
        case _ as unreachable:
            assert_never(unreachable)
    return bytes(encoded)


def write_raster(
    pixels: NDArray[np.uint8],
    dest: Path,
    fmt: OutputFormat | None = None,
    *,
    lossless: bool = False,
) -> Path:
    chosen = DEFAULT_OUTPUT_FORMAT if fmt is None else fmt
    if chosen is OutputFormat.HEIC:
        raise CodecMissing(
            "HEIC output uses the system codec only. Install the platform HEIF codec."
        )
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(encode_raster(pixels, chosen, lossless=lossless))
    return dest
