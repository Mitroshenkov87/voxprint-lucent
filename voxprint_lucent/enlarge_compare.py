"""Before/after compare for an Enlarge result. Zoom and pan stay in lockstep."""

from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QImage, QMouseEvent, QPainter, QPaintEvent, QWheelEvent
from PySide6.QtWidgets import QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget

from voxprint_lucent.decode import DecodeError, decode_image
from voxprint_lucent.theme import build_stylesheet
from voxprint_lucent.theme_tokens import ACCENT, ROOT_PLAIN, TEXT


@dataclass(frozen=True)
class ViewState:
    zoom: float
    pan_x: float
    pan_y: float


def split_cut(width: int, slider: float) -> int:
    """View x where the before image ends and the after image starts."""
    if width <= 0:
        return 0
    fraction = min(max(slider, 0.0), 1.0)
    return min(max(int(round(width * fraction)), 0), width)


def zoom_at(state: ViewState, cursor_x: float, cursor_y: float, factor: float) -> ViewState:
    """Zoom both sides around the cursor. The image point under the cursor stays put."""
    zoom = min(max(state.zoom * factor, 0.1), 32.0)
    content_x = (cursor_x - state.pan_x) / state.zoom
    content_y = (cursor_y - state.pan_y) / state.zoom
    return ViewState(zoom, cursor_x - content_x * zoom, cursor_y - content_y * zoom)


def pan_by(state: ViewState, dx: float, dy: float) -> ViewState:
    return ViewState(state.zoom, state.pan_x + dx, state.pan_y + dy)


def paint_split(
    painter: QPainter,
    width: int,
    height: int,
    before: QImage,
    after: QImage,
    state: ViewState,
    slider: float,
) -> None:
    """Paint the split. The left of the cut is the source; the right is the result."""
    painter.setRenderHint(QPainter.RenderHint.Antialiasing, False)
    painter.fillRect(0, 0, width, height, QColor(ROOT_PLAIN))
    cut = split_cut(width, slider)
    _draw_half(painter, before, state, 0, cut, height)
    _draw_half(painter, after, state, cut, width - cut, height)
    painter.setPen(QColor(ACCENT))
    painter.drawLine(cut, 0, cut, max(height - 1, 0))


def _draw_half(
    painter: QPainter,
    image: QImage,
    state: ViewState,
    x: int,
    width: int,
    height: int,
) -> None:
    if width <= 0 or height <= 0 or image.isNull():
        return
    painter.save()
    painter.setClipRect(x, 0, width, height)
    painter.translate(state.pan_x, state.pan_y)
    painter.scale(state.zoom, state.zoom)
    painter.drawImage(0, 0, image)
    painter.restore()


def reveal_command(path: Path) -> list[str]:
    if sys.platform == "win32":
        return ["explorer", f"/select,{path}"]
    return ["xdg-open", str(path.parent)]


def discard_output(source: Path, output: Path) -> bool:
    """Delete the enlarged file. The source is never deleted."""
    if output.resolve() == source.resolve():
        return False
    output.unlink()
    return True


class CompareWindow(QWidget):
    def __init__(self, source: Path, output: Path) -> None:
        super().__init__()
        self.source = source
        self.output = output
        self._slider = 0.5
        self._state = ViewState(1.0, 0.0, 0.0)
        self._drag: tuple[float, float] | None = None
        self.setObjectName("lucent-window")
        self.setWindowTitle("Compare")
        self.setStyleSheet(build_stylesheet())
        try:
            self._before = decode_image(source)
            self._after = decode_image(output)
            note = ""
        except (DecodeError, OSError, RuntimeError) as exc:
            self._before = QImage()
            self._after = QImage()
            note = str(exc)
        self._view = _SplitView(self)
        self._status = QLabel(note or f"{source.name}  |  {output.name}")
        self._status.setWordWrap(True)
        self._status.setStyleSheet(f"color: {TEXT};")
        keep = QPushButton("Keep")
        keep.setObjectName("compare-keep")
        keep.clicked.connect(self.keep)
        reveal = QPushButton("Reveal")
        reveal.setObjectName("compare-reveal")
        reveal.clicked.connect(self.reveal)
        discard = QPushButton("Discard")
        discard.setObjectName("compare-discard")
        discard.clicked.connect(self.discard)
        row = QHBoxLayout()
        row.addWidget(keep)
        row.addWidget(reveal)
        row.addWidget(discard)
        layout = QVBoxLayout(self)
        layout.addWidget(self._view, 1)
        layout.addWidget(self._status)
        layout.addLayout(row)
        self.resize(720, 480)

    def keep(self) -> bool:
        self._status.setText(str(self.output))
        return self.output.is_file()

    def discard(self) -> bool:
        if not discard_output(self.source, self.output):
            self._status.setText("Discard leaves the original in place.")
            return False
        self._status.setText("Discarded the enlarged file.")
        return True

    def reveal(self) -> None:
        subprocess.Popen(reveal_command(self.output))

    def composed(self) -> QImage:
        image = QImage(self._view.width(), self._view.height(), QImage.Format.Format_RGB32)
        painter = QPainter(image)
        paint_split(
            painter,
            image.width(),
            image.height(),
            self._before,
            self._after,
            self._state,
            self._slider,
        )
        painter.end()
        return image

    def set_slider(self, slider: float) -> None:
        self._slider = min(max(slider, 0.0), 1.0)
        self._view.update()

    def nudge(self, dx: float, dy: float) -> None:
        self._state = pan_by(self._state, dx, dy)
        self._view.update()

    def zoom(self, cursor_x: float, cursor_y: float, factor: float) -> None:
        self._state = zoom_at(self._state, cursor_x, cursor_y, factor)
        self._view.update()


class _SplitView(QWidget):
    def __init__(self, compare: CompareWindow) -> None:
        super().__init__(compare)
        self._compare = compare
        self.setMinimumSize(320, 200)
        self.setMouseTracking(True)

    def paintEvent(self, _event: QPaintEvent) -> None:
        painter = QPainter(self)
        paint_split(
            painter,
            self.width(),
            self.height(),
            self._compare._before,
            self._compare._after,
            self._compare._state,
            self._compare._slider,
        )
        painter.end()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        self._compare._drag = (event.position().x(), event.position().y())
        left = event.button() == Qt.MouseButton.LeftButton
        plain = event.modifiers() == Qt.KeyboardModifier.NoModifier
        if left and plain:
            width = max(self.width(), 1)
            self._compare.set_slider(event.position().x() / width)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self._compare._drag is None:
            return
        if event.buttons() & Qt.MouseButton.RightButton:
            x, y = self._compare._drag
            self._compare.nudge(event.position().x() - x, event.position().y() - y)
            self._compare._drag = (event.position().x(), event.position().y())
            return
        if event.buttons() & Qt.MouseButton.LeftButton:
            width = max(self.width(), 1)
            self._compare.set_slider(event.position().x() / width)

    def mouseReleaseEvent(self, _event: QMouseEvent) -> None:
        self._compare._drag = None

    def wheelEvent(self, event: QWheelEvent) -> None:
        steps = event.angleDelta().y() / 120
        if steps == 0:
            return
        factor = 1.1 ** steps
        self._compare.zoom(event.position().x(), event.position().y(), factor)
