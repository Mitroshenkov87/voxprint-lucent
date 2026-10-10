"""On-demand PySide6 process for settings, tools, the info panel, Enlarge, and updates.

The C++ viewer does not launch this module. Cold start stays in the C++ executable.
"""

from __future__ import annotations

import argparse
import sys


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
    args = parser.parse_args(argv)
    if args.ping:
        print("pong")
        return 0
    # Startup-budget exception to the no-inline-import rule: --ping must not
    # load Qt. This process is never on the viewer's cold-start path.
    from voxprint_lucent.theme import build_stylesheet

    sheet = build_stylesheet()
    if "#60a5fa" not in sheet or "#f5d76e" not in sheet:
        print("Shared theme did not load.", file=sys.stderr)
        return 1
    print("tools-host-ready")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
