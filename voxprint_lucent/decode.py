"""Decode an image to QImage. Qt plugins first; imagecodecs only for AVIF and JPEG XL."""

from __future__ import annotations

from pathlib import Path

from PySide6.QtGui import QColorSpace, QImage, QImageReader

_QT_SUFFIXES = {
    ".jpg",
    ".jpeg",
    ".jpe",
    ".png",
    ".gif",
    ".bmp",
    ".webp",
    ".tif",
    ".tiff",
    ".ico",
    ".tga",
    ".svg",
    ".svgz",
}
_HEIF_SUFFIXES = {".heif", ".heic"}
_MODERN_SUFFIXES = {".avif", ".jxl"}
_CODEC_FALLBACK_SUFFIXES = _MODERN_SUFFIXES | {".webp"}


class DecodeError(Exception):
    def __init__(self, path: Path, message: str) -> None:
        self.path = path
        super().__init__(message)


def decode_image(path: Path) -> QImage:
    """Return a display image. EXIF orientation is applied. Missing HEIF codec is an error."""
    suffix = path.suffix.lower()
    if suffix in _MODERN_SUFFIXES:
        image = _decode_modern(path, suffix)
    else:
        image = _decode_qt(path)
        # Qt's WebP plugin rejects some valid tiny files. imagecodecs is the backup.
        if image.isNull() and suffix in _CODEC_FALLBACK_SUFFIXES:
            image = _decode_modern(path, suffix)
    if image.isNull():
        if suffix in _HEIF_SUFFIXES:
            raise DecodeError(
                path,
                "Could not open this HEIF/HEIC file. Install the platform HEIF codec.",
            )
        raise DecodeError(path, f"Could not decode {path.name}")
    return _to_srgb(image)


def _decode_qt(path: Path) -> QImage:
    reader = QImageReader(str(path))
    reader.setAutoTransform(True)
    return reader.read()


def _decode_modern(path: Path, suffix: str) -> QImage:
    # Lazy: NumPy and imagecodecs stay off the JPEG/WebP startup path.
    import imagecodecs
    import numpy as np

    data = path.read_bytes()
    if suffix == ".avif":
        decoded = imagecodecs.avif_decode(data)
    elif suffix == ".jxl":
        decoded = imagecodecs.jpegxl_decode(data)
    elif suffix == ".webp":
        decoded = imagecodecs.webp_decode(data)
    else:
        return QImage()
    array = _as_uint8(np.ascontiguousarray(decoded))
    return _array_to_qimage(array)


def _as_uint8(array):  # type: ignore[no-untyped-def]
    import numpy as np

    if array.dtype == np.uint8:
        return array
    if np.issubdtype(array.dtype, np.floating):
        scaled = np.clip(array, 0.0, 1.0) * 255.0
        return scaled.astype(np.uint8)
    if np.issubdtype(array.dtype, np.integer):
        shift = max(0, array.dtype.itemsize * 8 - 8)
        return (array >> shift).astype(np.uint8)
    raise DecodeError(Path(array.dtype.name), f"Unsupported pixel type {array.dtype}")


def _array_to_qimage(array):  # type: ignore[no-untyped-def]
    if array.ndim == 2:
        height, width = array.shape
        image = QImage(array.data, width, height, width, QImage.Format.Format_Grayscale8)
        return image.copy()
    if array.ndim != 3:
        return QImage()
    height, width, channels = array.shape
    if channels == 3:
        fmt = QImage.Format.Format_RGB888
        stride = width * 3
    elif channels == 4:
        fmt = QImage.Format.Format_RGBA8888
        stride = width * 4
    else:
        return QImage()
    image = QImage(array.data, width, height, stride, fmt)
    return image.copy()


def _to_srgb(image: QImage) -> QImage:
    space = image.colorSpace()
    if not space.isValid():
        return image
    target = QColorSpace(QColorSpace.NamedColorSpace.SRgb)
    if space == target:
        return image
    converted = image.copy()
    if not converted.convertToColorSpace(target):
        return image
    return converted
