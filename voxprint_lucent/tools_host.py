"""On-demand PySide6 process for settings, tools, the info panel, Enlarge, and updates.

The C++ viewer starts this process when the user opens tools. It is not on the
path that paints the first image.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from voxprint_lucent.network_policy import UpdateCheckError, check_for_update


def main(argv: list[str] | None = None) -> int:
    """Run the tools host and return the process status."""
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
    parser.add_argument(
        "--frame",
        type=int,
        default=0,
        help="Frame index to enlarge when the file has more than one. A still ignores this.",
    )
    parser.add_argument(
        "--check-update",
        action="store_true",
        help="Read this repository's GitHub releases and print a newer tag.",
    )
    args = parser.parse_args(argv)
    if args.ping:
        print("pong")
        return 0
    if args.check_update:
        try:
            offer = check_for_update()
        except UpdateCheckError as exc:
            print(f"update check failed: {exc}", flush=True)
            return 1
        if offer is None:
            print("up to date", flush=True)
            return 0
        print(f"{offer['tag']} {offer['html_url']}", flush=True)
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

        return run_enlarge(path, frame_index=args.frame)
    app = QApplication([])
    _ = build_stylesheet()
    window = ToolsWindow(path)
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
