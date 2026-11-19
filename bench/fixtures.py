"""Deterministic 12 megapixel fixtures. Cached on disk, not committed."""

from __future__ import annotations

from pathlib import Path

WIDTH = 4000
HEIGHT = 3000


def ensure_fixtures(root: Path) -> dict[str, Path]:
    root.mkdir(parents=True, exist_ok=True)
    paths = {
        "jpeg": root / "large.jpg",
        "jpeg_next": root / "large-next.jpg",
        "webp": root / "large.webp",
        "avif": root / "large.avif",
        "jxl": root / "large.jxl",
    }
    ready = root / "READY"
    complete = all(path.is_file() and path.stat().st_size > 0 for path in paths.values())
    if ready.is_file() and complete:
        return paths
    _write_all(paths)
    ready.write_text("4000x3000 seed=20261010\n", encoding="utf-8")
    return paths


def _write_all(paths: dict[str, Path]) -> None:
    import imagecodecs
    import numpy as np

    image = _pattern(np.random.default_rng(20261010))
    nxt = _pattern(np.random.default_rng(20261011))
    paths["jpeg"].write_bytes(bytes(imagecodecs.jpeg_encode(image, level=90)))
    paths["jpeg_next"].write_bytes(bytes(imagecodecs.jpeg_encode(nxt, level=90)))
    paths["webp"].write_bytes(bytes(imagecodecs.webp_encode(image, level=80)))
    paths["avif"].write_bytes(bytes(imagecodecs.avif_encode(image, speed=8)))
    paths["jxl"].write_bytes(bytes(imagecodecs.jpegxl_encode(image, effort=3)))


def _pattern(rng):  # type: ignore[no-untyped-def]
    import numpy as np

    y = np.linspace(0, 1, HEIGHT, dtype=np.float32)[:, None]
    x = np.linspace(0, 1, WIDTH, dtype=np.float32)[None, :]
    base = np.empty((HEIGHT, WIDTH, 3), np.float32)
    base[:, :, 0] = 255.0 * x
    base[:, :, 1] = 255.0 * y
    base[:, :, 2] = 255.0 * (1.0 - x) * (0.35 + 0.65 * y)
    block = rng.integers(0, 40, size=(HEIGHT // 8, WIDTH // 8, 3), dtype=np.uint8)
    noise = np.repeat(np.repeat(block, 8, axis=0), 8, axis=1).astype(np.float32)
    mixed = np.clip(base + noise[:HEIGHT, :WIDTH] - 20.0, 0, 255)
    return mixed.astype(np.uint8)
