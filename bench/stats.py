"""Sample statistics for the cold-start harness.

For v0.1.x the cold pass rule is 450 ms, with cached next/previous at 30 ms.
Warm JPEG and WebP stay at 250 ms. A warm full-resolution AVIF or JPEG XL
at 12 megapixels or larger uses 350 ms. Run-M0 records those warm figures.
"""

from __future__ import annotations

import math

COLD_FIRST_PIXEL_BUDGET_MS = 450
WARM_FIRST_PIXEL_BUDGET_MS = 250
FIRST_PIXEL_BUDGET_MS = WARM_FIRST_PIXEL_BUDGET_MS
NAVIGATE_BUDGET_MS = 30
FULL_RES_WARM_BUDGET_MS = 350


def median(samples: list[float]) -> float:
    if not samples:
        raise ValueError("median of an empty sample")
    ordered = sorted(samples)
    count = len(ordered)
    mid = count // 2
    if count % 2 == 1:
        return ordered[mid]
    return (ordered[mid - 1] + ordered[mid]) / 2


def percentile(samples: list[float], percent: float) -> float:
    """Nearest-rank percentile. ``percent`` is in 0..100."""
    if not samples:
        raise ValueError("percentile of an empty sample")
    if not 0 <= percent <= 100:
        raise ValueError("percent must be between 0 and 100")
    ordered = sorted(samples)
    if len(ordered) == 1:
        return ordered[0]
    rank = math.ceil(percent / 100 * len(ordered))
    index = min(len(ordered) - 1, max(0, rank - 1))
    return ordered[index]


def summarise(samples: list[float]) -> dict[str, float | int]:
    return {
        "n": len(samples),
        "median_ms": median(samples),
        "p95_ms": percentile(samples, 95),
        "min_ms": min(samples),
        "max_ms": max(samples),
    }


def warm_budget_ms(image_format: str) -> int:
    """Warm median budget for one full-resolution image.

    AVIF and JPEG XL at 12 megapixels or larger use 350 ms. Every other
    format uses the 250 ms warm budget. The v0.1.x cold gate is 450 ms.
    """
    if image_format in {"avif", "jxl"}:
        return FULL_RES_WARM_BUDGET_MS
    return WARM_FIRST_PIXEL_BUDGET_MS


def evaluate(
    *,
    cold_median_ms: float | None,
    navigate_median_ms: float | None,
    reference_laptop: bool,
) -> str:
    """Pass only on the reference laptop: cold median <= 450 and cached nav <= 30.

    Other machines, including CI, are ``indicative``. Warm budgets (250 ms,
    or 350 ms for a 12 megapixel AVIF or JPEG XL) do not change this verdict.
    """
    if not reference_laptop:
        return "indicative"
    if cold_median_ms is None or navigate_median_ms is None:
        return "fail"
    if cold_median_ms <= COLD_FIRST_PIXEL_BUDGET_MS and navigate_median_ms <= NAVIGATE_BUDGET_MS:
        return "pass"
    return "fail"
