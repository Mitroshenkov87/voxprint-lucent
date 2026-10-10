"""Argument parsing stays free of Qt."""

from __future__ import annotations

from voxprint_lucent.cli import parse_args


def test_image_and_export_default() -> None:
    args = parse_args(["photo.jpg", "--export"])
    assert args.export == "AUTO"
    assert args.format is None
    assert args.benchmark is False


def test_benchmark_neighbour() -> None:
    args = parse_args(["a.jpg", "--benchmark", "--navigate-next", "b.jpg"])
    assert args.benchmark is True
    assert args.navigate_next.name == "b.jpg"
