"""Network policy. The viewer startup path does not import this module.

Allowed: an update check against this repository's GitHub releases, and
requests the user triggers. Statistics and diagnostics are off unless the
user opts in. Nothing here runs while an image is opening.
"""

from __future__ import annotations

import json
import re
import urllib.request
from collections.abc import Mapping
from typing import Any

from voxprint_lucent import __version__

# Public releases collection. No query string, so it cannot carry a usage token.
# /releases/latest hides pre-releases, so the check reads this list instead.
RELEASES_FEED_URL = "https://api.github.com/repos/Mitroshenkov87/voxprint-lucent/releases"

# Product name only. No version, OS, locale, or install id.
UPDATE_USER_AGENT = "VoxprintAILucent"

DIAGNOSTICS_DEFAULT = False
STATISTICS_DEFAULT = False
BACKGROUND_TELEMETRY = False

_VERSION = re.compile(r"^(\d+)\.(\d+)\.(\d+)(?:rc(\d+))?$")


class UpdateCheckError(RuntimeError):
    """The releases feed could not be read."""


def diagnostics_enabled(settings: Mapping[str, object]) -> bool:
    return bool(settings.get("diagnostics_opt_in", False))


def statistics_enabled(settings: Mapping[str, object]) -> bool:
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


def fetch_releases() -> list[dict[str, Any]]:
    """GET the public releases list. No query string and no identifiers."""
    request = update_check_request()
    outgoing = urllib.request.Request(
        request["url"],
        headers={"User-Agent": request["user_agent"], "Accept": request["accept"]},
        method=request["method"],
    )
    try:
        with urllib.request.urlopen(outgoing, timeout=15) as response:
            payload = json.load(response)
    except (OSError, json.JSONDecodeError, UnicodeError) as exc:
        raise UpdateCheckError(str(exc)) from exc
    if not isinstance(payload, list):
        raise UpdateCheckError("releases feed was not a list")
    return [item for item in payload if isinstance(item, dict)]


def check_for_update(current: str | None = None) -> dict[str, Any] | None:
    """Return a newer release, or None when this install is current."""
    if current is None:
        current = __version__
    return choose_update(current, fetch_releases())
