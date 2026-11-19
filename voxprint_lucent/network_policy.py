"""Network policy. The viewer startup path does not import this module.

Allowed: an update check against this repository's GitHub releases, and
requests the user triggers. Statistics and diagnostics are off unless the
user opts in. Nothing here runs while an image is opening.
"""

from __future__ import annotations

import json
import os
import re
import sys
import urllib.error
import urllib.request
from collections.abc import Mapping
from pathlib import Path
from typing import Any

from voxprint_lucent import __version__

# Public releases collection. No query string, so it cannot carry a usage token.
# /releases/latest hides pre-releases, so the check reads this list instead.
RELEASES_FEED_URL = "https://api.github.com/repos/Mitroshenkov87/voxprint-lucent/releases"
ENLARGE_RELEASES_FEED_URL = "https://api.github.com/repos/Mitroshenkov87/voxprint-enlarge/releases"
_ENLARGE_DOWNLOAD_PREFIX = "https://github.com/Mitroshenkov87/voxprint-enlarge/releases/download/"

# Product name only. No version, OS, locale, or install id.
UPDATE_USER_AGENT = "VoxprintAILucent"

DIAGNOSTICS_DEFAULT = False
STATISTICS_DEFAULT = False
BACKGROUND_TELEMETRY = False

_VERSION = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:rc(\d+))?$")


class UpdateCheckError(RuntimeError):
    """The releases feed could not be read."""


def diagnostics_enabled(settings: Mapping[str, object]) -> bool:
    """Return whether the user turned diagnostics on. Off unless the setting says so."""
    return bool(settings.get("diagnostics_opt_in", False))


def statistics_enabled(settings: Mapping[str, object]) -> bool:
    """Return whether the user turned statistics on. Off unless the setting says so."""
    return bool(settings.get("statistics_opt_in", False))


def update_check_request() -> dict[str, str]:
    """Describe the update GET. The caller must be the user, or a later daily check.

    This function does not send the request.
    """
    return {
        "method": "GET",
        "url": RELEASES_FEED_URL,
        "user_agent": UPDATE_USER_AGENT,
        "accept": "application/vnd.github+json",
    }


def version_key(text: str) -> tuple[int, int, int, int, int]:
    """Order versions so ``0.1.0rc1`` is older than ``0.1.0``."""
    raw = text.strip()
    if raw[:1] in "vV":
        raw = raw[1:]
    raw = raw.replace("-", "")
    match = _VERSION.fullmatch(raw)
    if match is None:
        return (0, 0, 0, 0, 0)
    major, minor, patch, rc = match.groups()
    if rc is None:
        return (int(major), int(minor), int(patch), 1, 0)
    return (int(major), int(minor), int(patch), 0, int(rc))


def is_prerelease_version(text: str) -> bool:
    """Return whether `text` names a pre-release."""
    raw = text.strip()
    if raw[:1] in "vV":
        raw = raw[1:]
    return "rc" in raw.replace("-", "")


def choose_update(current: str, releases: list[dict[str, Any]]) -> dict[str, Any] | None:
    """Newest GitHub release newer than ``current``.

    A stable install ignores pre-releases. A pre-release install can move to
    a newer pre-release or to the stable version that follows it.
    """
    current_key = version_key(current)
    allow_pre = is_prerelease_version(current)
    best_key = current_key
    best: dict[str, Any] | None = None
    for item in releases:
        if item.get("draft"):
            continue
        if item.get("prerelease") and not allow_pre:
            continue
        tag = item.get("tag_name")
        if not isinstance(tag, str):
            continue
        key = version_key(tag)
        if key <= best_key:
            continue
        best_key = key
        best = item
    if best is None:
        return None
    url = best.get("html_url")
    return {
        "tag": str(best.get("tag_name")),
        "html_url": url if isinstance(url, str) else "",
        "prerelease": bool(best.get("prerelease")),
    }


def _fetch_release_list(url: str) -> list[dict[str, Any]]:
    """GET a public releases list. No query string and no identifiers."""
    outgoing = urllib.request.Request(
        url,
        headers={"User-Agent": UPDATE_USER_AGENT, "Accept": "application/vnd.github+json"},
        method="GET",
    )
    try:
        with urllib.request.urlopen(outgoing, timeout=15) as response:
            payload = json.load(response)
    except (OSError, json.JSONDecodeError, UnicodeError) as exc:
        raise UpdateCheckError(str(exc)) from exc
    if not isinstance(payload, list):
        raise UpdateCheckError("releases feed was not a list")
    return [item for item in payload if isinstance(item, dict)]


def fetch_releases() -> list[dict[str, Any]]:
    """GET this repository's public releases list."""
    return _fetch_release_list(RELEASES_FEED_URL)


def enlarge_asset_name(platform: str | None = None) -> str | None:
    """Installer file published for this operating system, if Enlarge ships one."""
    system = sys.platform if platform is None else platform
    if system == "win32":
        return "VoxprintEnlarge-Setup.exe"
    if system == "linux":
        return "voxprint-enlarge-linux-x64.tar.gz"
    return None


def choose_enlarge_package(
    releases: list[dict[str, Any]], platform: str | None = None
) -> dict[str, Any] | None:
    """Newest Enlarge release asset for `platform`.

    The download URL has to stay on the Enlarge releases host. A missing size
    is reported as 0 so the confirm dialog can omit it.
    """
    name = enlarge_asset_name(platform)
    if name is None:
        return None
    for item in releases:
        if item.get("draft"):
            continue
        assets = item.get("assets")
        if not isinstance(assets, list):
            continue
        for asset in assets:
            if not isinstance(asset, dict) or asset.get("name") != name:
                continue
            url = asset.get("browser_download_url")
            if not isinstance(url, str) or not url.startswith(_ENLARGE_DOWNLOAD_PREFIX):
                continue
            size = asset.get("size")
            reported = size if isinstance(size, int) and size > 0 else 0
            tag = item.get("tag_name")
            return {
                "name": name,
                "bytes": reported,
                "url": url,
                "tag": tag if isinstance(tag, str) else "",
            }
    return None


def enlarge_package(platform: str | None = None) -> dict[str, Any] | None:
    """Release metadata for the Enlarge package, or None when it is not listed."""
    return choose_enlarge_package(_fetch_release_list(ENLARGE_RELEASES_FEED_URL), platform)


def install_enlarge_package(dest_dir: Path) -> int:
    """Download the Enlarge release package into `dest_dir` and print progress lines.

    Progress is JSON on stdout, the same shape the viewer already reads. Cancel
    is the process being killed. A failed download removes the partial file.
    """
    try:
        package = enlarge_package()
    except UpdateCheckError as exc:
        print(json.dumps({"type": "error", "message": str(exc)}), flush=True)
        return 1
    if package is None:
        print(json.dumps({"type": "error", "message": "no package"}), flush=True)
        return 1
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / str(package["name"])
    partial = dest.with_name(dest.name + ".partial")
    total = int(package["bytes"])
    print(
        json.dumps({"type": "meta", "bytes": total, "path": str(dest), "partial": str(partial)}),
        flush=True,
    )
    request = urllib.request.Request(
        str(package["url"]),
        headers={"User-Agent": UPDATE_USER_AGENT},
        method="GET",
    )
    got = 0
    next_pct = 0
    try:
        with urllib.request.urlopen(request, timeout=60) as response, partial.open("wb") as out:
            header = response.headers.get("Content-Length")
            if total <= 0 and isinstance(header, str) and header.isdigit():
                total = int(header)
            while True:
                chunk = response.read(256 * 1024)
                if not chunk:
                    break
                out.write(chunk)
                got += len(chunk)
                if total > 0:
                    pct = min(100, got * 100 // total)
                    if pct >= next_pct:
                        line = json.dumps({"type": "progress", "stage": "download", "pct": pct})
                        print(line, flush=True)
                        next_pct = pct + 1
                else:
                    print(json.dumps({"type": "progress", "stage": "download"}), flush=True)
            out.flush()
            os.fsync(out.fileno())
    except (OSError, urllib.error.URLError) as exc:
        partial.unlink(missing_ok=True)
        print(json.dumps({"type": "error", "message": str(exc)}), flush=True)
        return 1
    partial.replace(dest)
    print(json.dumps({"type": "done", "path": str(dest), "pct": 100}), flush=True)
    return 0


def check_for_update(current: str | None = None) -> dict[str, Any] | None:
    """Return a newer release, or None when this install is current."""
    if current is None:
        current = __version__
    return choose_update(current, fetch_releases())
