"""On-demand PySide6 process for settings, tools, the info panel, Enlarge, and updates.

The C++ viewer starts this process when the user opens tools. It is not on the
path that paints the first image.
"""

from __future__ import annotations

import argparse


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="voxprint-lucent-tools",
        description="Voxprint AI Lucent tools host. Not part of viewer startup.",
    )
    parser.add_argument(
        "--ping",
        action="store_true",
        help="Answer without loading Qt. Used to prove this process is separate.",
    )
    parser.add_argument("--path", default="", help="Image the viewer had open.")
    args = parser.parse_args(argv)
    if args.ping:
        print("pong")
        return 0
    # Startup-budget exception to the no-inline-import rule: --ping must not
    # load Qt. This process is never on the viewer's cold-start path.
    from PySide6.QtWidgets import QApplication, QLabel, QVBoxLayout, QWidget

    from voxprint_lucent.theme import build_stylesheet

    app = QApplication([])
    window = QWidget()
    window.setObjectName("lucent-window")
    window.setWindowTitle("Voxprint AI Lucent")
    window.setStyleSheet(build_stylesheet())
    layout = QVBoxLayout(window)
    layout.addWidget(QLabel("Tools"))
    if args.path:
        layout.addWidget(QLabel(args.path))
    layout.addWidget(
        QLabel(
            "Settings, the info panel, Enlarge, and updates open here. "
            "They are not loaded to paint the first image."
        )
    )
    window.resize(520, 180)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
