"""Frameless viewer window. Qt is imported here, not from the process entry module."""

from __future__ import annotations

import sys
import time
from pathlib import Path

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget

from voxprint_lucent.cache import ImageCache
from voxprint_lucent.decode import DecodeError, decode_image
from voxprint_lucent.rhi_view import ImageRhiWidget
from voxprint_lucent.theme import build_stylesheet


class ViewerWindow(QWidget):
    def __init__(
        self,
        image: QImage,
        *,
        source: Path,
        benchmark: bool = False,
        navigate_next: Path | None = None,
    ) -> None:
        super().__init__()
        self.setObjectName("lucent-window")
        self.setWindowTitle("Voxprint AI Lucent")
        self.setWindowFlags(Qt.WindowType.FramelessWindowHint | Qt.WindowType.Window)
        self.setStyleSheet(build_stylesheet())
        self.resize(1280, 720)
        self._benchmark = benchmark
        self._navigate_next = navigate_next
        self._cache = ImageCache()
        self._cache.put(source, image)
        self._first_done = False
        self._await_nav = False
        self._nav_token = 0
        self._nav_t0 = 0.0
        self.view = ImageRhiWidget()
        layout = QVBoxLayout(self)
        layout.setContentsMargins(2, 2, 2, 2)
        layout.addWidget(self.view)
        self.view.frameSubmitted.connect(self._on_frame)
        self.view.renderFailed.connect(self._on_render_failed)
        self.view.set_image(image)

    def keyPressEvent(self, event) -> None:  # noqa: N802 - Qt override
        if event.key() in (Qt.Key.Key_Escape, Qt.Key.Key_Q):
            self.close()
            return
        super().keyPressEvent(event)

    def _on_render_failed(self) -> None:
        print("The graphics device failed before the first pixel.", file=sys.stderr)
        QApplication.exit(4)

    def _on_frame(self) -> None:
        if self.view.presented_generation <= 0:
            return
        if not self._first_done:
            self._first_done = True
            if self._benchmark:
                sys.stdout.write("FIRST_PIXEL\n")
                sys.stdout.flush()
            if self._navigate_next is not None:
                QTimer.singleShot(0, self._begin_navigate)
            elif self._benchmark:
                QTimer.singleShot(0, QApplication.quit)
            return
        if self._await_nav and self.view.presented_generation == self._nav_token:
            elapsed_ms = (time.perf_counter() - self._nav_t0) * 1000
            sys.stdout.write(f"NAVIGATE_MS {elapsed_ms:.3f}\n")
            sys.stdout.flush()
            self._await_nav = False
            if self._benchmark:
                QTimer.singleShot(0, QApplication.quit)

    def _begin_navigate(self) -> None:
        assert self._navigate_next is not None
        cached = self._cache.get(self._navigate_next)
        if cached is None:
            cached = decode_image(self._navigate_next)
            self._cache.put(self._navigate_next, cached)
        self._nav_t0 = time.perf_counter()
        self.view.set_image(cached)
        self._nav_token = self.view.generation
        self._await_nav = True


def run_viewer(args) -> int:  # type: ignore[no-untyped-def]
    path: Path | None = args.image
    if path is None:
        print("usage: voxprint-lucent IMAGE", file=sys.stderr)
        return 2
    if not path.is_file():
        print(f"No such image: {path}", file=sys.stderr)
        return 3
    app = QApplication.instance() or QApplication([])
    try:
        image = decode_image(path)
    except DecodeError as exc:
        print(str(exc), file=sys.stderr)
        return 3
    window = ViewerWindow(
        image,
        source=path,
        benchmark=bool(args.benchmark),
        navigate_next=args.navigate_next,
    )
    window.show()
    return int(app.exec())
