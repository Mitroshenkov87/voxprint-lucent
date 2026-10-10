"""Sample statistics for the cold-start harness."""

from __future__ import annotations

import math


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


def evaluate(
    *,
    cold_median_ms: float | None,
    navigate_median_ms: float | None,
    reference_laptop: bool,
) -> str:
    """Plan rule: pass only on the reference laptop, cold median <= 250 and cached nav <= 30.

    Other machines, including CI, are ``indicative``.
    """
    if not reference_laptop:
        return "indicative"
    if cold_median_ms is None or navigate_median_ms is None:
        return "fail"
    if cold_median_ms <= 250 and navigate_median_ms <= 30:
        return "pass"
    return "fail"
