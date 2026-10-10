"""Small decoded-image cache. The first paint does not wait for neighbours."""

from __future__ import annotations

from collections import OrderedDict
from pathlib import Path

from PySide6.QtGui import QImage


class ImageCache:
    def __init__(self, limit: int = 5) -> None:
        self._limit = limit
        self._items: OrderedDict[Path, QImage] = OrderedDict()

    def put(self, path: Path, image: QImage) -> None:
        key = path.resolve()
        self._items.pop(key, None)
        self._items[key] = image
        while len(self._items) > self._limit:
            self._items.popitem(last=False)

    def get(self, path: Path) -> QImage | None:
        key = path.resolve()
        image = self._items.get(key)
        if image is not None:
            self._items.move_to_end(key)
        return image

    def __len__(self) -> int:
        return len(self._items)
