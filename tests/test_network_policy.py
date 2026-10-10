"""Diagnostics stay off, and the update description carries no usage token."""

from __future__ import annotations

from voxprint_lucent.network_policy import (
    BACKGROUND_TELEMETRY,
    DIAGNOSTICS_DEFAULT,
    STATISTICS_DEFAULT,
    UPDATE_USER_AGENT,
    diagnostics_enabled,
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
