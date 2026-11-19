"""Skip GPU tests when this process has no display."""

from __future__ import annotations

import os
import sys

import pytest


def has_display() -> bool:
    if sys.platform == "win32":
        return True
    return bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    if has_display():
        return
    skip = pytest.mark.skip(reason="No display for the Qt RHI viewer")
    for item in items:
        if "gui" in item.keywords:
            item.add_marker(skip)
