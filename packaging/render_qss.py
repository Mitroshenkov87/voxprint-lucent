"""Generate Lucent's stylesheet from the vendored Yesod tokens.

Equivalent to ``python -m voxprint_lucent.theme_tokens --share``.
"""

from __future__ import annotations

from voxprint_lucent.theme_tokens import main

if __name__ == "__main__":
    raise SystemExit(main())
