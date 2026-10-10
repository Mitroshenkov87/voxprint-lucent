"""Enlarge actions inside the tools process. Hidden when no usable GPU is reported."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from PySide6.QtWidgets import QApplication, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget

from voxprint_lucent.enlarge_client import EnlargeClient, EnlargeError, EnlargeResult
from voxprint_lucent.enlarge_compare import CompareWindow
from voxprint_lucent.enlarge_protocol import action_tooltip, action_visible
from voxprint_lucent.theme import build_stylesheet


class EnlargeWindow(QWidget):
    def __init__(self, path: Path, client: EnlargeClient | None = None) -> None:
        super().__init__()
        self._path = path
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
        if action_visible(self._caps):
            self._status.setText("Ready.")
            row = QHBoxLayout()
            quick = QPushButton("Quick enlarge")
            quick.setObjectName("quick-enlarge")
            quick.setToolTip(action_tooltip(self._caps))
            quick.clicked.connect(self.run_quick)
            open_button = QPushButton("Open in Enlarge")
            open_button.setObjectName("open-enlarge")
            open_button.setToolTip(action_tooltip(self._caps))
            open_button.clicked.connect(self.run_open)
            cancel = QPushButton("Cancel")
            cancel.setObjectName("cancel-enlarge")
            cancel.clicked.connect(self.cancel)
            row.addWidget(quick)
            row.addWidget(open_button)
            row.addWidget(cancel)
            layout.addLayout(row)
        else:
            self._status.setText(action_tooltip(self._caps))
        self.resize(560, 180)

    def status_text(self) -> str:
        return self._status.text()

    def run_quick(self) -> CompareWindow:
        return self._run(open_ui=False)

    def run_open(self) -> CompareWindow:
        return self._run(open_ui=True)

    def cancel(self) -> None:
        self._client.cancel()

    def shutdown(self) -> None:
        self._client.shutdown()

    def _run(self, *, open_ui: bool) -> CompareWindow:
        try:
            result = self._client.enlarge(
                self._path,
                open_ui=open_ui,
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


def run_enlarge(path: Path) -> int:
    app = QApplication.instance() or QApplication([])
    window = EnlargeWindow(path)
    window.show()
    return app.exec()
