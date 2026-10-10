"""Median, p95, and the M0 pass rule."""

from __future__ import annotations

import pytest

from bench.stats import evaluate, median, percentile, summarise


def test_median_odd_and_even() -> None:
    assert median([3, 1, 2]) == 2
    assert median([1, 2, 3, 4]) == 2.5


def test_nearest_rank_p95() -> None:
    samples = list(range(1, 11))
    assert percentile(samples, 95) == 10
    assert percentile([5], 95) == 5


def test_summary_keys() -> None:
    summary = summarise([10, 30, 20])
    assert summary["n"] == 3
    assert summary["median_ms"] == 20


def test_pass_rule_requires_the_reference_laptop() -> None:
    assert evaluate(cold_median_ms=200, navigate_median_ms=10, reference_laptop=True) == "pass"
    assert evaluate(cold_median_ms=251, navigate_median_ms=10, reference_laptop=True) == "fail"
    assert evaluate(cold_median_ms=200, navigate_median_ms=31, reference_laptop=True) == "fail"
    indicative = evaluate(cold_median_ms=200, navigate_median_ms=10, reference_laptop=False)
    assert indicative == "indicative"
    assert evaluate(cold_median_ms=None, navigate_median_ms=10, reference_laptop=True) == "fail"


def test_percentile_rejects_empty() -> None:
    with pytest.raises(ValueError):
        percentile([], 95)
