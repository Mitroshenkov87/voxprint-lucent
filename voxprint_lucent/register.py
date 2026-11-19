"""Register Lucent in the suite registry through voxprint-common.

Lucent ships no models, so there is no mdl.json and no model bookkeeping
here; this only records the app row in ``state/installed-apps.json``.
"""

from __future__ import annotations

import argparse
import importlib.metadata
import json
import sys
from pathlib import Path

APP_ID = "lucent"
DISPLAY_NAME = "VOXPrint AI Lucent"
FALLBACK_VERSION = "0.8.0"


def _default_install_dir() -> Path:
    """Package parent normally, two levels up for a frozen app layout."""
    package_dir = Path(__file__).resolve().parent
    if getattr(sys, "frozen", False) or hasattr(sys, "_MEIPASS"):
        return package_dir.parent.parent
    return package_dir.parent


def _default_version() -> str:
    """The installed ``voxprint-lucent`` version, else the fallback."""
    try:
        return importlib.metadata.version("voxprint-lucent")
    except importlib.metadata.PackageNotFoundError:
        return FALLBACK_VERSION


def _already_registered(install_dir: Path, version: str) -> bool:
    """True when the registry already holds this id, path, and version."""
    try:
        from voxprint_common.paths import state_dir

        registry = state_dir() / "installed-apps.json"
        payload = json.loads(registry.read_text(encoding="utf-8"))
    except (ImportError, OSError, ValueError):
        return False
    if not isinstance(payload, dict):
        return False
    rows = payload.get("apps")
    if not isinstance(rows, list):
        return False
    wanted = str(Path(install_dir).resolve())
    for row in rows:
        if not isinstance(row, dict) or row.get("id") != APP_ID:
            continue
        stored = str(row.get("path", ""))
        same_path = stored == wanted or (stored and str(Path(stored).resolve()) == wanted)
        if same_path and str(row.get("version", "")) == version:
            return True
    return False


def register(install_dir: Path, version: str) -> None:
    """Record Lucent in ``installed-apps.json``. Idempotent and cheap.

    Args:
        install_dir: Directory Lucent is installed in.
        version: Installed version string.

    Raises:
        Exception: voxprint-common rejected the registration.
    """
    if _already_registered(install_dir, version):
        return
    from voxprint_common.mdl.catalog import register_app

    register_app(
        APP_ID,
        Path(install_dir),
        display_name=DISPLAY_NAME,
        version=version,
    )


def _unregister() -> bool:
    """Remove the Lucent row. Returns True when a row was removed."""
    from voxprint_common.mdl.catalog import unregister_app

    return unregister_app(APP_ID)


def build_parser() -> argparse.ArgumentParser:
    """Build the registry argument parser."""
    parser = argparse.ArgumentParser(
        prog="python -m voxprint_lucent.register",
        description="Register VOXPrint AI Lucent in the suite registry.",
    )
    parser.add_argument(
        "--path",
        type=Path,
        default=None,
        help="Install directory. Defaults to the package parent, or two levels up when frozen.",
    )
    parser.add_argument(
        "--version",
        default=None,
        help=(
            "Version to record. Defaults to the installed voxprint-lucent version, "
            f"else {FALLBACK_VERSION}."
        ),
    )
    parser.add_argument(
        "--unregister",
        action="store_true",
        help="Remove the Lucent row from the registry.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print what happened. Success is silent without this flag.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Run the registry CLI and return the process status."""
    args = build_parser().parse_args(argv)
    try:
        if args.unregister:
            removed = _unregister()
            if args.verbose:
                print("unregistered" if removed else "not registered")
            return 0
        install_dir = args.path if args.path is not None else _default_install_dir()
        version = args.version if args.version is not None else _default_version()
        register(install_dir, version)
        if args.verbose:
            print(f"registered {APP_ID} {version} at {install_dir}")
        return 0
    except Exception as exc:
        print(f"register failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
