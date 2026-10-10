"""Command-line parsing. Stdlib only, so importing it does not load Qt."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


def public_build_label() -> str:
    """``0.1.0 RC · build N "Codename"``. N is ``GITHUB_RUN_NUMBER`` plus ``BUILD.json`` offset."""
    root = Path(__file__).resolve().parents[1]
    payload = json.loads((root / "BUILD.json").read_text(encoding="utf-8"))
    offset = int(payload["offset"])
    raw = os.environ.get("GITHUB_RUN_NUMBER", "").strip()
    run = int(raw) if raw else 0
    codename = str(payload["codename"])
    return f'0.1.0 RC · build {run + offset} "{codename}"'


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
