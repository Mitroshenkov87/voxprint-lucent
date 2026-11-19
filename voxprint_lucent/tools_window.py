"""On-demand tools window. Not constructed to paint the viewer's first image."""

from __future__ import annotations

import subprocess
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QMessageBox,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from voxprint_lucent.edits import (
    color_text,
    crop_to_avif,
    export_frame,
    read_frame,
    reveal_command,
    rotate_or_flip,
    send_to_trash,
)
from voxprint_lucent.formats import PngRejected
from voxprint_lucent.info import ImageFacts, facts_for
from voxprint_lucent.theme import build_stylesheet
from voxprint_lucent.tools_text import label


class _Preview(QLabel):
    def __init__(self, pick) -> None:  # type: ignore[no-untyped-def]
        super().__init__()
        self._pick = pick
        self.setMinimumSize(320, 240)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)

    def mousePressEvent(self, event) -> None:  # type: ignore[no-untyped-def]
        self._pick(event)


class ToolsWindow(QWidget):
    """Info panel and light edits for the image the viewer opened."""
    def __init__(self, path: Path) -> None:
        """Open the panel on `path`."""
        super().__init__()
        self._path = path
        self._frame = 0
        self._image = QImage()
        self._playing = False
        self.setObjectName("lucent-window")
        self.setWindowTitle(label("title"))
        self.setStyleSheet(build_stylesheet())
        self._info = QLabel()
        self._info.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        self._info.setWordWrap(True)
        self._color = QLabel(label("color"))
        self._status = QLabel()
        self._status.setWordWrap(True)
        self._preview = _Preview(self._pick)
        self._timer = QTimer(self)
        self._timer.setInterval(100)
        self._timer.timeout.connect(self._advance)
        self._crop_x, self._crop_y, self._crop_w, self._crop_h = (
            QSpinBox(),
            QSpinBox(),
            QSpinBox(),
            QSpinBox(),
        )
        for box in (self._crop_x, self._crop_y, self._crop_w, self._crop_h):
            box.setRange(0, 100000)
        self._format = QComboBox()
        self._format.addItems(["avif", "jxl", "webp"])
        root = QHBoxLayout(self)
        root.addWidget(self._preview, 1)
        side = QVBoxLayout()
        side.addWidget(QLabel(label("info")))
        side.addWidget(self._info)
        side.addWidget(self._color)
        side.addWidget(QLabel(label("frames")))
        side.addLayout(self._frame_row())
        side.addLayout(self._edit_row())
        side.addLayout(self._crop_row())
        side.addWidget(self._status)
        root.addLayout(side)
        self.resize(960, 640)
        self.reload()

    def reload(self) -> None:
        """Reread the file and refresh the panel."""
        facts = facts_for(self._path, self._frame)
        self._image = read_frame(self._path, self._frame)
        self._info.setText(self._facts_text(facts))
        self._preview.setPixmap(_pixmap(self._image))
        self._crop_w.setValue(facts.width)
        self._crop_h.setValue(facts.height)
        self._frame_count = facts.frame_count

    def info_text(self) -> str:
        """Return the text currently shown in the info panel."""
        return self._info.text()

    def _facts_text(self, facts: ImageFacts) -> str:
        return "\n".join(
            [
                facts.path,
                f"{facts.width}×{facts.height}",
                facts.format_name,
                f"{facts.bit_depth}-bit",
                facts.color_profile,
                facts.exif_line,
                facts.gps,
                f"{facts.frame_index + 1}/{facts.frame_count}",
            ]
        )

    def _frame_row(self) -> QHBoxLayout:
        row = QHBoxLayout()
        self._play = QPushButton(label("play"))
        self._play.clicked.connect(self._toggle_play)
        previous = QPushButton(label("prev"))
        previous.clicked.connect(lambda: self._step(-1))
        nxt = QPushButton(label("next"))
        nxt.clicked.connect(lambda: self._step(1))
        export = QPushButton(label("export"))
        export.clicked.connect(self._export)
        row.addWidget(self._play)
        row.addWidget(previous)
        row.addWidget(nxt)
        row.addWidget(self._format)
        row.addWidget(export)
        return row

    def _edit_row(self) -> QHBoxLayout:
        row = QHBoxLayout()
        actions = [
            (label("rotate"), lambda: self._transform("cw")),
            (label("flip_h"), lambda: self._transform("flip-h")),
            (label("flip_v"), lambda: self._transform("flip-v")),
            (label("copy_path"), self._copy_path),
            (label("copy_image"), self._copy_image),
            (label("trash"), self._trash),
            (label("reveal"), self._reveal),
        ]
        for text, slot in actions:
            button = QPushButton(text)
            button.clicked.connect(slot)
            row.addWidget(button)
        return row

    def _crop_row(self) -> QFormLayout:
        form = QFormLayout()
        form.addRow("x", self._crop_x)
        form.addRow("y", self._crop_y)
        form.addRow("w", self._crop_w)
        form.addRow("h", self._crop_h)
        button = QPushButton(label("crop"))
        button.clicked.connect(self._crop)
        form.addRow(button)
        return form

    def _toggle_play(self) -> None:
        self._playing = not self._playing
        if self._playing and getattr(self, "_frame_count", 1) > 1:
            self._play.setText(label("pause"))
            self._timer.start()
            return
        self._playing = False
        self._play.setText(label("play"))
        self._timer.stop()

    def _advance(self) -> None:
        self._step(1)

    def _step(self, delta: int) -> None:
        count = getattr(self, "_frame_count", 1)
        if count <= 1:
            return
        self._frame = (self._frame + delta) % count
        self.reload()

    def _export(self) -> None:
        try:
            dest = export_frame(self._path, self._frame, self._format.currentText())
        except (PngRejected, ValueError, OSError) as exc:
            self._status.setText(str(exc))
            return
        self._status.setText(str(dest))

    def _transform(self, operation: str) -> None:
        dest = rotate_or_flip(self._path, operation)
        self._path = dest
        self._frame = 0
        self.reload()
        self._status.setText(str(dest))

    def _crop(self) -> None:
        dest = crop_to_avif(
            self._path,
            self._crop_x.value(),
            self._crop_y.value(),
            self._crop_w.value(),
            self._crop_h.value(),
            self._frame,
        )
        self._status.setText(str(dest))

    def _copy_path(self) -> None:
        QApplication.clipboard().setText(str(self._path.resolve()))
        self._status.setText(str(self._path.resolve()))

    def _copy_image(self) -> None:
        QApplication.clipboard().setImage(self._image)
        self._status.setText(label("copy_image"))

    def _trash(self) -> None:
        answer = QMessageBox.question(self, label("trash"), self._path.name)
        if answer != QMessageBox.StandardButton.Yes:
            return
        send_to_trash(self._path)
        self._status.setText(label("trash"))

    def _reveal(self) -> None:
        subprocess.Popen(reveal_command(self._path))

    def _pick(self, event) -> None:  # type: ignore[no-untyped-def]
        shown = self._preview.pixmap()
        if shown is None:
            return
        point = sample_pixel(
            (self._preview.width(), self._preview.height()),
            (shown.width(), shown.height()),
            (self._image.width(), self._image.height()),
            (event.position().x(), event.position().y()),
        )
        if point is None:
            return
        px, py = point
        color = self._image.pixelColor(px, py)
        hex_text, rgb = color_text(color.red(), color.green(), color.blue())
        self._color.setText(f"{hex_text}  {rgb}")
        QApplication.clipboard().setText(hex_text)


def sample_pixel(
    label_size: tuple[int, int],
    pixmap_size: tuple[int, int],
    image_size: tuple[int, int],
    point: tuple[float, float],
) -> tuple[int, int] | None:
    """Map a click on the centered preview to a source pixel."""
    label_w, label_h = label_size
    pixmap_w, pixmap_h = pixmap_size
    image_w, image_h = image_size
    if pixmap_w <= 0 or pixmap_h <= 0 or image_w <= 0 or image_h <= 0:
        return None
    local_x = point[0] - (label_w - pixmap_w) / 2
    local_y = point[1] - (label_h - pixmap_h) / 2
    if local_x < 0 or local_y < 0 or local_x >= pixmap_w or local_y >= pixmap_h:
        return None
    px = min(int(local_x * image_w / pixmap_w), image_w - 1)
    py = min(int(local_y * image_h / pixmap_h), image_h - 1)
    return px, py


def _pixmap(image: QImage) -> QPixmap:
    scaled = image.scaled(
        480,
        480,
        Qt.AspectRatioMode.KeepAspectRatio,
        Qt.TransformationMode.SmoothTransformation,
    )
    return QPixmap.fromImage(scaled)
