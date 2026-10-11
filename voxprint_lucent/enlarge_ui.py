"""Enlarge actions inside the tools process.

The action stays visible. It is disabled when Enlarge is missing or reports no
usable GPU. A missing install offers an Install Enlarge button.
"""

from __future__ import annotations

import tempfile
from pathlib import Path
from typing import Any

from PySide6.QtCore import QUrl
from PySide6.QtGui import QDesktopServices, QImageReader
from PySide6.QtWidgets import QApplication, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget

from voxprint_lucent.enlarge_client import EnlargeClient, EnlargeError, EnlargeResult
from voxprint_lucent.enlarge_compare import CompareWindow
from voxprint_lucent.enlarge_protocol import (
    ENLARGE_RELEASES_URL,
    action_enabled,
    action_tooltip,
    install_offered,
)
from voxprint_lucent.theme import build_stylesheet


def frame_count(path: Path) -> int:
    """How many frames Qt reports. A still image, or an unreadable file, is 1."""
    count = QImageReader(str(path)).imageCount()
    return count if count > 1 else 1


def still_for_enlarge(path: Path, frame_index: int = 0) -> Path:
    """Absolute still for Enlarge. A multi-frame file is written as one PNG frame.

    Enlarge rejects a stack of frames. The current frame is written as a PNG
    under the temp directory, and that file is what the hand-off sends.
    """
    source = path.resolve()
    if frame_count(source) <= 1:
        return source
    index = frame_index if frame_index > 0 else 0
    reader = QImageReader(str(source))
    reader.setAutoTransform(True)
    if index > 0 and not reader.jumpToImage(index):
        raise EnlargeError("io_error", f"Could not read frame {index + 1}.", False)
    image = reader.read()
    if image.isNull():
        raise EnlargeError("io_error", "Could not read the current frame.", False)
    folder = Path(tempfile.gettempdir()) / "voxprint-lucent"
    folder.mkdir(parents=True, exist_ok=True)
    dest = folder / f"{source.stem}-frame-{index + 1}.png"
    if not image.save(str(dest), "PNG"):
        raise EnlargeError("io_error", "Could not save the current frame as a still.", False)
    return dest.resolve()


class EnlargeWindow(QWidget):
    """Window that starts an Enlarge job for one image."""
    def __init__(
        self,
        path: Path,
        client: EnlargeClient | None = None,
        frame_index: int = 0,
    ) -> None:
        """Bind the window to `path` and an optional client."""
        super().__init__()
        self._path = path
        self._frame_index = frame_index if frame_index > 0 else 0
        self._frames = frame_count(path)
        self._client = client or EnlargeClient()
        self._compare: CompareWindow | None = None
        self.setObjectName("lucent-window")
        self.setWindowTitle("Enlarge")
        self.setStyleSheet(build_stylesheet())
        self._caps = self._client.probe()
        self._status = QLabel()
        self._status.setWordWrap(True)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(path.name))
        layout.addWidget(self._status)
        enabled = action_enabled(self._caps)
        status = "Ready." if enabled else action_tooltip(self._caps)
        if self._frames > 1:
            status = f"{status} Sending frame {self._frame_index + 1} of {self._frames} as a still."
        self._status.setText(status)
        row = QHBoxLayout()
        quick = QPushButton("Quick enlarge")
        quick.setObjectName("quick-enlarge")
        quick.setEnabled(enabled)
        quick.setToolTip(action_tooltip(self._caps))
        quick.clicked.connect(self.run_quick)
        open_button = QPushButton("Open in Enlarge")
        open_button.setObjectName("open-enlarge")
        open_button.setEnabled(enabled)
        open_button.setToolTip(action_tooltip(self._caps))
        open_button.clicked.connect(self.run_open)
        cancel = QPushButton("Cancel")
        cancel.setObjectName("cancel-enlarge")
        cancel.setEnabled(enabled)
        cancel.clicked.connect(self.cancel)
        row.addWidget(quick)
        row.addWidget(open_button)
        row.addWidget(cancel)
        if install_offered(self._caps):
            install = QPushButton("Install Enlarge")
            install.setObjectName("install-enlarge")
            install.clicked.connect(self.open_releases)
            row.addWidget(install)
        layout.addLayout(row)
        self.resize(560, 180)

    def status_text(self) -> str:
        """Return the status line currently shown."""
        return self._status.text()

    def run_quick(self) -> CompareWindow:
        """Enlarge at 2x and open the compare view."""
        return self._run(scale=2)

    def run_open(self) -> CompareWindow:
        """Enlarge with the CLI default and open the compare view."""
        return self._run(scale=None)

    def cancel(self) -> None:
        """Cancel the job in progress."""
        self._client.cancel()

    @staticmethod
    def open_releases() -> None:
        """Open the Enlarge releases page in the browser."""
        QDesktopServices.openUrl(QUrl(ENLARGE_RELEASES_URL))

    def shutdown(self) -> None:
        """Stop a local Enlarge process this window started."""
        self._client.shutdown()

    def _run(self, *, scale: int | None) -> CompareWindow:
        try:
            source = still_for_enlarge(self._path, self._frame_index)
            result = self._client.enlarge(
                source,
                scale=scale,
                on_event=self._on_event,
            )
        except EnlargeError as exc:
            self._status.setText(f"{exc.code}: {exc}")
            raise
        return self._show_result(result)

    def _on_event(self, message: dict[str, Any]) -> None:
        if message.get("type") != "progress":
            return
        self._status.setText(f"{message.get('stage', '')} {message.get('pct', '')}%")

    def _show_result(self, result: EnlargeResult) -> CompareWindow:
        self._status.setText(str(result.output))
        compare = CompareWindow(self._path, result.output)
        compare.show()
        self._compare = compare
        return compare


def run_enlarge(path: Path, frame_index: int = 0) -> int:
    """Show the Enlarge window for `path` and run until it closes."""
    app = QApplication.instance() or QApplication([])
    window = EnlargeWindow(path, frame_index=frame_index)
    window.show()
    return app.exec()
