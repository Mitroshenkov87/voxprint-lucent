"""The window paints an image, and --benchmark reports the first pixel."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.gui


def _solid_jpeg(path: Path, red: int, green: int, blue: int) -> None:
    import imagecodecs
    import numpy as np

    pixels = np.zeros((32, 32, 3), dtype=np.uint8)
    pixels[:, :, 0] = red
    pixels[:, :, 1] = green
    pixels[:, :, 2] = blue
    path.write_bytes(bytes(imagecodecs.jpeg_encode(pixels, level=95)))


def test_benchmark_prints_first_pixel_and_navigate(tmp_path: Path) -> None:
    first = tmp_path / "a.jpg"
    second = tmp_path / "b.jpg"
    _solid_jpeg(first, 220, 10, 10)
    _solid_jpeg(second, 10, 10, 220)
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent",
            "--benchmark",
            str(first),
            "--navigate-next",
            str(second),
        ],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    assert "FIRST_PIXEL" in proc.stdout
    assert "NAVIGATE_MS" in proc.stdout


def test_missing_file_exits_3(tmp_path: Path) -> None:
    proc = subprocess.run(
        [sys.executable, "-m", "voxprint_lucent", str(tmp_path / "nope.jpg")],
        capture_output=True,
        text=True,
        timeout=20,
        check=False,
    )
    assert proc.returncode == 3


def test_export_defaults_to_avif(tmp_path: Path) -> None:
    source = tmp_path / "photo.jpg"
    _solid_jpeg(source, 20, 180, 40)
    proc = subprocess.run(
        [sys.executable, "-m", "voxprint_lucent", str(source), "--export"],
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert proc.returncode == 0, proc.stderr
    written = Path(proc.stdout.strip())
    assert written.name == "photo.avif"
    assert written.is_file()
    assert written.stat().st_size > 0


def test_export_png_is_rejected(tmp_path: Path) -> None:
    source = tmp_path / "photo.jpg"
    _solid_jpeg(source, 1, 2, 3)
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "voxprint_lucent",
            str(source),
            "--export",
            str(tmp_path / "out.png"),
            "--format",
            "png",
        ],
        capture_output=True,
        text=True,
        timeout=20,
        check=False,
    )
    assert proc.returncode == 2
    assert not (tmp_path / "out.png").exists()


def test_painted_pixel_matches_the_image(tmp_path: Path) -> None:
    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication

    from voxprint_lucent.decode import decode_image
    from voxprint_lucent.viewer import ViewerWindow

    path = tmp_path / "red.jpg"
    _solid_jpeg(path, 240, 0, 0)
    app = QApplication.instance() or QApplication([])
    window = ViewerWindow(decode_image(path), source=path)
    window.view.setFixedSize(64, 64)
    window.show()
    grabbed: dict[str, object] = {}

    def _grab() -> None:
        frame = window.view.grabFramebuffer()
        color = frame.pixelColor(frame.width() // 2, frame.height() // 2)
        grabbed["color"] = (color.red(), color.green(), color.blue())
        app.quit()

    QTimer.singleShot(500, _grab)
    QTimer.singleShot(5000, app.quit)
    app.exec()
    red, green, blue = grabbed["color"]
    assert red > 200
    assert green < 40
    assert blue < 40
