"""The Support the Project section is the Movie Dubber block, byte for byte."""

from __future__ import annotations

import hashlib
from pathlib import Path

_SHA256 = "ae6900ccf5c80ef10bd55e5f4a6807ea86d72341e8c2ff831f554c3e56364053"
_LENGTH = 481
_HEADING = b"## \xe2\x98\x95 Support the Project"


def test_readme_support_section_matches_the_source_bytes() -> None:
    readme = Path("README.md").read_bytes()
    start = readme.index(_HEADING)
    section = readme[start : start + _LENGTH]
    assert len(section) == _LENGTH
    assert hashlib.sha256(section).hexdigest() == _SHA256
    assert section.endswith(b"\n")
