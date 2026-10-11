"""Command-line parsing. Stdlib only, so importing it does not load Qt."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def load_build_payload(start: Path | None = None) -> dict[str, Any]:
    """Read the nearest ``BUILD.json`` at or above `start`.

    A checkout keeps the file at the repository root. The Windows measurement
    harness copies it next to the staged package, which is the parent of this
    module, so the search starts there and walks upward.
    """
    origin = (start or Path(__file__)).resolve()
    directories = [origin] if origin.is_dir() else []
    directories.extend(origin.parents)
    for directory in directories:
        candidate = directory / "BUILD.json"
        if candidate.is_file():
            loaded = json.loads(candidate.read_text(encoding="utf-8"))
            if isinstance(loaded, dict):
                return loaded
    raise FileNotFoundError(f"BUILD.json not found from {origin}")


def public_build_label() -> str:
    """``0.1.0 RC · build N "Codename"``. N is ``BUILD.json`` ``number``."""
    payload = load_build_payload()
    if "number" in payload:
        number = int(payload["number"])
    else:
        number = int(payload["offset"])
    codename = str(payload["codename"])
    return f'0.1.0 RC · build {number} "{codename}"'


def build_parser() -> argparse.ArgumentParser:
    """Build the viewer argument parser without loading Qt."""
    parser = argparse.ArgumentParser(
        prog="voxprint-lucent",
        description="Voxprint AI Lucent, a fast colour-accurate image viewer.",
    )
    parser.add_argument("image", nargs="?", type=Path, help="Image file to open")
    parser.add_argument(
        "--benchmark",
        action="store_true",
        help="Paint the image, print FIRST_PIXEL, then quit",
    )
    parser.add_argument(
        "--navigate-next",
        type=Path,
        default=None,
        help="With --benchmark, time a switch to this already-decoded neighbour",
    )
    parser.add_argument(
        "--export",
        nargs="?",
        const="AUTO",
        default=None,
        help="Save a copy and quit. Default format is AVIF.",
    )
    parser.add_argument(
        "--format",
        default=None,
        help="Output format for --export: avif (default), jxl, webp, jpeg, heic. PNG is rejected.",
    )
    parser.add_argument(
        "--lossless",
        action="store_true",
        help="Lossless JPEG XL or WebP when that format is selected",
    )
    parser.add_argument("--version", action="version", version=public_build_label())
    return parser


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    """Parse `argv`, or the process arguments when `argv` is omitted."""
    return build_parser().parse_args(argv)
