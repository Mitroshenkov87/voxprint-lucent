"""Enlarge actions inside the tools process.

The action stays visible. It is disabled when Enlarge is missing or reports no
usable GPU. A missing install offers an Install Enlarge button.
"""

from __future__ import annotations

import tempfile
from pathlib import Path
from typing import Any

from PySide6.QtCore import QObject, QThread, QUrl, Signal
from PySide6.QtGui import QDesktopServices, QImageReader
from PySide6.QtWidgets import QApplication, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget

from voxprint_lucent.enlarge_client import EnlargeClient, EnlargeError, EnlargeResult
from voxprint_lucent.enlarge_compare import CompareWindow
from voxprint_lucent.enlarge_protocol import (
    ENLARGE_RELEASES_URL,
    Capabilities,
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


class _ProbeWorker(QObject):
    """Run the Enlarge capability probe off the UI thread."""

    finished = Signal(object)

    def __init__(self, client: EnlargeClient) -> None:
        super().__init__()
        self._client = client

    def run(self) -> None:
        """Probe, then drop the worker. The signal carries the capabilities or None."""
        caps = self._client.probe()
        self.finished.emit(caps)


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
        self._caps: Capabilities | None = None
        self._gate = False
        self._probe_thread: QThread | None = None
        self._probe_worker: _ProbeWorker | None = None
        self.setObjectName("lucent-window")
        self.setWindowTitle("Enlarge")
        self.setStyleSheet(build_stylesheet())
        self._status = QLabel()
        self._status.setWordWrap(True)
        layout = QVBoxLayout(self)
        layout.addWidget(QLabel(path.name))
        layout.addWidget(self._status)
        row = QHBoxLayout()
        self._check = QPushButton("Check")
        self._check.setObjectName("check-gate")
        self._check.clicked.connect(self.run_check)
        self._quick = QPushButton("Quick enlarge")
        self._quick.setObjectName("quick-enlarge")
        self._quick.clicked.connect(self.run_quick)
        self._open = QPushButton("Open in Enlarge")
        self._open.setObjectName("open-enlarge")
        self._open.clicked.connect(self.run_open)
        self._cancel = QPushButton("Cancel")
        self._cancel.setObjectName("cancel-enlarge")
        self._cancel.clicked.connect(self.cancel)
        row.addWidget(self._check)
        row.addWidget(self._quick)
        row.addWidget(self._open)
        row.addWidget(self._cancel)
        self._install: QPushButton | None = None
        if not self._client.command:
            self._install = QPushButton("Install Enlarge")
            self._install.setObjectName("install-enlarge")
            self._install.clicked.connect(self.open_releases)
            row.addWidget(self._install)
        layout.addLayout(row)
        self._mark_red()
        self.resize(560, 180)

    def _mark_red(self) -> None:
        self._gate = False
        self._check.setProperty("gate", "red")
        self._check.style().unpolish(self._check)
        self._check.style().polish(self._check)
        self._quick.setEnabled(False)
        self._open.setEnabled(False)
        self._cancel.setEnabled(False)
        if not self._client.command:
            self._status.setText("Enlarge is not installed.")
            return
        self._status.setText("Check the hand-off before enlarging.")

    def note_edit(self) -> None:
        """An edit invalidates the last check. The gate goes red and the worker is gone."""
        self.cancel()
        self._caps = None
        self._mark_red()

    def run_check(self) -> None:
        """Probe Enlarge in the background. The gate stays red until the probe finishes."""
        if not self._client.command or self._probe_thread is not None:
            return
        self._status.setText("Checking…")
        self._cancel.setEnabled(True)
        thread = QThread(self)
        worker = _ProbeWorker(self._client)
        worker.moveToThread(thread)
        thread.started.connect(worker.run)
        worker.finished.connect(self._finish_check)
        worker.finished.connect(thread.quit)
        thread.finished.connect(worker.deleteLater)
        thread.finished.connect(thread.deleteLater)
        thread.finished.connect(self._clear_probe)
        self._probe_thread = thread
        self._probe_worker = worker
        thread.start()

    def _clear_probe(self) -> None:
        self._probe_thread = None
        self._probe_worker = None

    def _finish_check(self, caps: object) -> None:
        self._client.shutdown()
        if not self._gate and self._status.text() != "Checking…":
            return
        parsed = caps if isinstance(caps, Capabilities) else None
        self._caps = parsed
        enabled = action_enabled(parsed)
        self._gate = enabled
        self._check.setProperty("gate", "green" if enabled else "red")
        self._check.style().unpolish(self._check)
        self._check.style().polish(self._check)
        self._quick.setEnabled(enabled)
        self._open.setEnabled(enabled)
        tip = action_tooltip(parsed)
        self._quick.setToolTip(tip)
        self._open.setToolTip(tip)
        self._cancel.setEnabled(False)
        status = "Ready." if enabled else tip
        if self._frames > 1 and enabled:
            status = f"{status} Sending frame {self._frame_index + 1} of {self._frames} as a still."
        self._status.setText(status)
        if install_offered(parsed) and self._install is None and not self._client.command:
            install = QPushButton("Install Enlarge")
            install.setObjectName("install-enlarge")
            install.clicked.connect(self.open_releases)
            self.layout().itemAt(2).layout().addWidget(install)
            self._install = install

    def status_text(self) -> str:
        """Return the status line currently shown."""
        return self._status.text()

    def run_quick(self) -> CompareWindow:
        """Enlarge at 2x and open the compare view. Refuses while the gate is red."""
        if not self._gate:
            raise EnlargeError("not_checked", "Check the hand-off before enlarging.", False)
        return self._run(scale=2)

    def run_open(self) -> CompareWindow:
        """Enlarge with the CLI default and open the compare view. Refuses while the gate is red."""
        if not self._gate:
            raise EnlargeError("not_checked", "Check the hand-off before enlarging.", False)
        return self._run(scale=None)

    def cancel(self) -> None:
        """Cancel the job or the check. The probe process is unloaded."""
        if self._probe_thread is not None:
            self._status.setText("Cancelled.")
            self._probe_thread.requestInterruption()
            self._client.shutdown()
            self._probe_thread.quit()
        self._client.cancel()
        self._gate = False
        self._quick.setEnabled(False)
        self._open.setEnabled(False)

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
