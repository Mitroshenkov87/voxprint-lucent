"""Qt decodes JPEG and WebP. AVIF and JPEG XL go through imagecodecs."""

from __future__ import annotations

from pathlib import Path

import pytest

pytestmark = pytest.mark.gui


@pytest.fixture(scope="module")
def qapp():  # type: ignore[no-untyped-def]
    from PySide6.QtWidgets import QApplication

    app = QApplication.instance() or QApplication([])
    return app


def test_jpeg_and_webp_round_trip(qapp, tmp_path: Path) -> None:  # type: ignore[no-untyped-def]
    import imagecodecs
    import numpy as np

    from voxprint_lucent.decode import decode_image

    _ = qapp
    pixels = np.zeros((64, 64, 3), dtype=np.uint8)
    pixels[:, :, 1] = 180
    jpeg = tmp_path / "green.jpg"
    webp = tmp_path / "green.webp"
    jpeg.write_bytes(bytes(imagecodecs.jpeg_encode(pixels, level=95)))
    webp.write_bytes(bytes(imagecodecs.webp_encode(pixels, lossless=True)))
    for path in (jpeg, webp):
        image = decode_image(path)
        assert image.width() == 64
        assert image.height() == 64
        assert not image.isNull()


def test_avif_and_jxl(qapp, tmp_path: Path) -> None:  # type: ignore[no-untyped-def]
    import imagecodecs
    import numpy as np

    from voxprint_lucent.decode import decode_image

    _ = qapp
    pixels = np.full((6, 5, 3), 40, dtype=np.uint8)
    pixels[:, :, 2] = 220
    avif = tmp_path / "blue.avif"
    jxl = tmp_path / "blue.jxl"
    avif.write_bytes(bytes(imagecodecs.avif_encode(pixels, speed=10)))
    jxl.write_bytes(bytes(imagecodecs.jpegxl_encode(pixels, lossless=True)))
    for path in (avif, jxl):
        image = decode_image(path)
        assert (image.width(), image.height()) == (5, 6)


def test_missing_heif_is_a_codec_hint(qapp, tmp_path: Path) -> None:  # type: ignore[no-untyped-def]
    from voxprint_lucent.decode import DecodeError, decode_image

    _ = qapp
    path = tmp_path / "photo.heic"
    path.write_bytes(b"not a heif file")
    with pytest.raises(DecodeError, match="HEIF"):
        decode_image(path)
