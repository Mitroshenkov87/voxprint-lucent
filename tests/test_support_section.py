"""The Support the Project section is the Movie Dubber block, byte for byte."""

from __future__ import annotations

import hashlib
from pathlib import Path

_SHA256 = "5ed792422730ca357c6c5e807579dd848135119a382c57053dcf579bc22eecc1"
_LENGTH = 183
_HEADING = b"## \xe2\x98\x95 Support the Project"


def test_readme_support_section_matches_the_source_bytes() -> None:
    readme = Path("README.md").read_bytes()
    start = readme.index(_HEADING)
    section = readme[start : start + _LENGTH]
    assert len(section) == _LENGTH
    assert hashlib.sha256(section).hexdigest() == _SHA256
    assert section.endswith(b"\n")
