"""Diagnostics stay off, and the update description carries no usage token."""

from __future__ import annotations

from voxprint_lucent.network_policy import (
    BACKGROUND_TELEMETRY,
    DIAGNOSTICS_DEFAULT,
    ENLARGE_RELEASES_FEED_URL,
    RELEASES_FEED_URL,
    STATISTICS_DEFAULT,
    UPDATE_USER_AGENT,
    choose_enlarge_package,
    choose_update,
    diagnostics_enabled,
    enlarge_asset_name,
    statistics_enabled,
    update_check_request,
)


def test_defaults_are_off() -> None:
    assert DIAGNOSTICS_DEFAULT is False
    assert STATISTICS_DEFAULT is False
    assert BACKGROUND_TELEMETRY is False
    assert diagnostics_enabled({}) is False
    assert statistics_enabled({}) is False
    assert diagnostics_enabled({"diagnostics_opt_in": True}) is True


def test_update_request_is_a_plain_get() -> None:
    request = update_check_request()
    assert request["method"] == "GET"
    assert "github.com" in request["url"]
    assert "?" not in request["url"]
    assert request["user_agent"] == UPDATE_USER_AGENT
    assert request["user_agent"] == "VoxprintAILucent"
    assert request["url"] == RELEASES_FEED_URL
    assert request["url"].endswith("/voxprint-lucent/releases")


def test_stable_install_ignores_a_pre_release() -> None:
    releases = [
        {"tag_name": "v0.1.0-rc1", "prerelease": True, "html_url": "https://example.invalid/rc"},
        {"tag_name": "v0.1.0", "prerelease": False, "html_url": "https://example.invalid/stable"},
    ]
    assert choose_update("0.1.0", releases) is None
    assert choose_update("0.1.0rc1", releases) == {
        "tag": "v0.1.0",
        "html_url": "https://example.invalid/stable",
        "prerelease": False,
    }


def test_pre_release_install_can_move_to_a_newer_pre_release() -> None:
    releases = [
        {
            "tag_name": "v0.1.0-rc2",
            "prerelease": True,
            "draft": False,
            "html_url": "https://example.invalid/rc2",
        },
        {"tag_name": "v0.1.0-rc1", "prerelease": True, "html_url": "https://example.invalid/rc1"},
        {"tag_name": "v9.9.9", "draft": True, "prerelease": False, "html_url": "https://example.invalid/draft"},
    ]
    offer = choose_update("0.1.0rc1", releases)
    assert offer is not None
    assert offer["tag"] == "v0.1.0-rc2"


def test_enlarge_package_uses_the_release_asset_for_this_system() -> None:
    assert "?" not in ENLARGE_RELEASES_FEED_URL
    assert ENLARGE_RELEASES_FEED_URL.endswith("/voxprint-enlarge/releases")
    assert enlarge_asset_name("linux") == "voxprint-enlarge-linux-x64.tar.gz"
    assert enlarge_asset_name("win32") == "VoxprintEnlarge-Setup.exe"
    assert enlarge_asset_name("darwin") is None
    releases = [
        {
            "tag_name": "v0.2.0-rc.6",
            "draft": False,
            "assets": [
                {
                    "name": "voxprint-enlarge-linux-x64.tar.gz",
                    "size": 656892416,
                    "browser_download_url": "https://github.com/Mitroshenkov87/voxprint-enlarge/releases/download/v0.2.0-rc.6/voxprint-enlarge-linux-x64.tar.gz",
                },
                {
                    "name": "other.tar.gz",
                    "size": 10,
                    "browser_download_url": "https://example.invalid/other.tar.gz",
                },
            ],
        }
    ]
    package = choose_enlarge_package(releases, "linux")
    assert package is not None
    assert package["bytes"] == 656892416
    assert package["name"] == "voxprint-enlarge-linux-x64.tar.gz"
    assert choose_enlarge_package(releases, "darwin") is None
    poisoned = [
        {
            "draft": False,
            "assets": [
                {
                    "name": "voxprint-enlarge-linux-x64.tar.gz",
                    "size": 12,
                    "browser_download_url": "https://example.invalid/voxprint-enlarge-linux-x64.tar.gz",
                }
            ],
        }
    ]
    assert choose_enlarge_package(poisoned, "linux") is None
