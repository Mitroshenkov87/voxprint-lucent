"""On-demand PySide6 process for settings, tools, the info panel, Enlarge, and updates.

The C++ viewer starts this process when the user opens tools. It is not on the
path that paints the first image.
"""

from __future__ import annotations

import argparse
from pathlib import Path


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
    parser.add_argument(
        "--enlarge",
        action="store_true",
        help="Open the Enlarge hand-off for --path instead of the info panel.",
    )
    args = parser.parse_args(argv)
    if args.ping:
        print("pong")
        return 0
    path = Path(args.path)
    if not args.path or not path.is_file():
        if args.enlarge:
            print("usage: voxprint-lucent-tools --enlarge --path IMAGE", flush=True)
        else:
            print("usage: voxprint-lucent-tools --path IMAGE", flush=True)
        return 2
    # Startup-budget exception to the no-inline-import rule: --ping and a
    # missing path must not load Qt. This process is never on the viewer's
    # cold-start path. The info panel and Enlarge both import Qt, so they stay
    # behind this check.
    from PySide6.QtWidgets import QApplication

    from voxprint_lucent.theme import build_stylesheet
    from voxprint_lucent.tools_window import ToolsWindow

    if args.enlarge:
        from voxprint_lucent.enlarge_ui import run_enlarge

        return run_enlarge(path)
    app = QApplication([])
    _ = build_stylesheet()
    window = ToolsWindow(path)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
