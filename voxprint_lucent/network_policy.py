"""Network policy. This module does not open sockets.

Allowed later: an update check against our GitHub releases, and requests the
user triggers. Statistics and diagnostics are off unless the user opts in.
Nothing here runs at startup. ``startup`` does not import this module.
"""

from __future__ import annotations

from collections.abc import Mapping

# Public releases feed. No query string, so it cannot carry a usage token.
RELEASES_FEED_URL = "https://api.github.com/repos/Mitroshenkov87/voxprint-lucent/releases/latest"

# Product name only. No version, OS, locale, or install id.
UPDATE_USER_AGENT = "VoxprintAILucent"

DIAGNOSTICS_DEFAULT = False
STATISTICS_DEFAULT = False
BACKGROUND_TELEMETRY = False


def diagnostics_enabled(settings: Mapping[str, object]) -> bool:
    return bool(settings.get("diagnostics_opt_in", False))


def statistics_enabled(settings: Mapping[str, object]) -> bool:
    return bool(settings.get("statistics_opt_in", False))


def update_check_request() -> dict[str, str]:
    """Describe the update GET. The caller must be the user, or the future daily check.

    This function does not send the request.
    """
    return {
        "method": "GET",
        "url": RELEASES_FEED_URL,
        "user_agent": UPDATE_USER_AGENT,
        "accept": "application/vnd.github+json",
    }
