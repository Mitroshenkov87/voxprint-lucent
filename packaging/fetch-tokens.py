"""Refresh vendored shell data from the pinned voxprint-common commit.

The files are vendored so a build does not need the network. Run this when
`commit` in share/voxprint-common.pin.json changes. Lucent's own preset and
app catalogs are not in that tree and are left in place.
"""

from __future__ import annotations

import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHARE = ROOT / "share"
REPO = "https://raw.githubusercontent.com/Mitroshenkov87/voxprint-common"
API = "https://api.github.com/repos/Mitroshenkov87/voxprint-common/contents"


def _bytes(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=60) as response:
        return response.read()


def _download_tree(remote: str, commit: str, destination: Path) -> None:
    listing = json.loads(_bytes(f"{API}/{remote}?ref={commit}"))
    destination.mkdir(parents=True, exist_ok=True)
    for entry in listing:
        if entry.get("type") != "file" or not str(entry.get("name", "")).endswith(".json"):
            continue
        target = destination / entry["name"]
        if target.name == "lucent.json":
            continue
        target.write_bytes(_bytes(f"{REPO}/{commit}/{remote}/{entry['name']}"))
        print(f"wrote {target.relative_to(ROOT)}")


def main() -> int:
    """Download tokens, locale catalogs, and preset documents from the pin."""
    pin = json.loads((SHARE / "voxprint-common.pin.json").read_text(encoding="utf-8"))
    commit = pin["commit"]
    paths = pin["paths"]
    tokens = SHARE / "tokens.json"
    tokens.write_bytes(_bytes(f"{REPO}/{commit}/{paths['tokens']}"))
    print(f"wrote share/tokens.json from {commit}")
    _download_tree(paths["locales"], commit, SHARE / "locales")
    _download_tree(paths["presets"], commit, SHARE / "presets")
    same = json.dumps(pin, indent=2) + "\n"
    (SHARE / "tokens.pin.json").write_text(same, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
