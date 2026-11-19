"""Process entry.

Lazy imports below are the M0 startup budget from PLAN-v1: Qt, the decoder,
settings, export, and the network policy stay unloaded until that path runs.
They are not circular imports.
"""

from __future__ import annotations

import os
import sys

from voxprint_lucent.cli import parse_args


def prepare_environment() -> None:
    """Set quiet Qt logging unless the user already chose a rule."""
    os.environ.setdefault("QT_LOGGING_RULES", "*.debug=false")


def main(argv: list[str] | None = None) -> int:
    """Start the viewer or an export and return the process status."""
    prepare_environment()
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.export is not None:
        from voxprint_lucent.exporting import export_copy

        return export_copy(args)
    from voxprint_lucent.viewer import run_viewer

    return run_viewer(args)


if __name__ == "__main__":
    raise SystemExit(main())
