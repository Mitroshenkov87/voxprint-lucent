"""Letterbox a decoded image inside the view, in normalised device coordinates."""

from __future__ import annotations

import array


def contain_vertices(
    image_w: int,
    image_h: int,
    view_w: int,
    view_h: int,
    *,
    y_up: bool,
) -> list[tuple[float, float, float, float]]:
    """Six vertices (x, y, u, v) for two triangles that fit the image in the view.

    UVs put the first image row at the top of the widget. ``y_up`` follows
    ``QRhi.isYUpInNDC``: OpenGL is up, Vulkan and D3D12 are down.
    """
    if image_w <= 0 or image_h <= 0 or view_w <= 0 or view_h <= 0:
        half_x, half_y = 1.0, 1.0
    else:
        scale = min(view_w / image_w, view_h / image_h)
        half_x = (image_w * scale) / view_w
        half_y = (image_h * scale) / view_h
    x0, x1 = -half_x, half_x
    if y_up:
        y_top, y_bottom = half_y, -half_y
    else:
        y_top, y_bottom = -half_y, half_y
    return [
        (x0, y_bottom, 0.0, 1.0),
        (x1, y_bottom, 1.0, 1.0),
        (x0, y_top, 0.0, 0.0),
        (x0, y_top, 0.0, 0.0),
        (x1, y_bottom, 1.0, 1.0),
        (x1, y_top, 1.0, 0.0),
    ]


def vertices_bytes(vertices: list[tuple[float, float, float, float]]) -> bytes:
    flat = array.array("f")
    for x, y, u, v in vertices:
        flat.extend((x, y, u, v))
    return flat.tobytes()
