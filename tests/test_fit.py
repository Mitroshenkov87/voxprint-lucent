"""Letterbox math for the GPU quad."""

from __future__ import annotations

from voxprint_lucent.fit import contain_vertices


def test_square_image_fills_a_square_view() -> None:
    vertices = contain_vertices(100, 100, 100, 100, y_up=True)
    xs = [vertex[0] for vertex in vertices]
    ys = [vertex[1] for vertex in vertices]
    assert min(xs) == -1
    assert max(xs) == 1
    assert min(ys) == -1
    assert max(ys) == 1


def test_wide_image_is_letterboxed_vertically() -> None:
    vertices = contain_vertices(200, 100, 100, 100, y_up=True)
    ys = [vertex[1] for vertex in vertices]
    xs = [vertex[0] for vertex in vertices]
    assert max(xs) == 1
    assert max(ys) == 0.5


def test_y_down_flips_the_top_vertex() -> None:
    up = contain_vertices(10, 10, 10, 10, y_up=True)
    down = contain_vertices(10, 10, 10, 10, y_up=False)
    # First vertex is the bottom edge when Y is up, and the top edge when Y is down.
    assert up[0][1] == -1
    assert down[0][1] == 1
    assert up[0][3] == 1.0
    assert down[0][3] == 1.0
