"""Refresh share/tokens.json from the pinned voxprint-common release.

The file is vendored so a build does not need the network. Run this when the
pin in share/tokens.pin.json changes.
"""

from __future__ import annotations

import hashlib
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    """Download the pinned tokens.json and refuse a hash mismatch."""
    pin = json.loads((ROOT / "share" / "tokens.pin.json").read_text(encoding="utf-8"))
    url = (
        "https://raw.githubusercontent.com/Mitroshenkov87/voxprint-common/"
        f"{pin['commit']}/{pin['path']}"
    )
    with urllib.request.urlopen(url, timeout=30) as response:
        body = response.read()
    digest = hashlib.sha256(body).hexdigest()
    if digest != pin["sha256"]:
        raise SystemExit(f"downloaded sha256 {digest} != pin {pin['sha256']}")
    (ROOT / "share" / "tokens.json").write_bytes(body)
    print(f"wrote share/tokens.json from voxprint-common {pin['version']} {pin['codename']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
