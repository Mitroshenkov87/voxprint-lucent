"""Labels for the tools window. English is the default."""

from __future__ import annotations

import os

_TEXT = {
    "en": {
        "title": "Tools",
        "info": "Info",
        "frames": "Frames",
        "play": "Play",
        "pause": "Pause",
        "prev": "Previous frame",
        "next": "Next frame",
        "export": "Export frame",
        "rotate": "Rotate",
        "flip_h": "Flip horizontal",
        "flip_v": "Flip vertical",
        "crop": "Crop to AVIF",
        "copy_path": "Copy path",
        "copy_image": "Copy image",
        "trash": "Move to trash",
        "reveal": "Reveal in folder",
        "color": "Colour",
    },
    "ru": {
        "title": "Инструменты",
        "info": "Сведения",
        "frames": "Кадры",
        "play": "Пуск",
        "pause": "Пауза",
        "prev": "Предыдущий кадр",
        "next": "Следующий кадр",
        "export": "Экспорт кадра",
        "rotate": "Поворот",
        "flip_h": "Отразить по горизонтали",
        "flip_v": "Отразить по вертикали",
        "crop": "Обрезать в AVIF",
        "copy_path": "Копировать путь",
        "copy_image": "Копировать изображение",
        "trash": "В корзину",
        "reveal": "Показать в папке",
        "color": "Цвет",
    },
    "de": {
        "title": "Werkzeuge",
        "info": "Info",
        "frames": "Frames",
        "play": "Wiedergabe",
        "pause": "Pause",
        "prev": "Vorheriger Frame",
        "next": "Nächster Frame",
        "export": "Frame exportieren",
        "rotate": "Drehen",
        "flip_h": "Horizontal spiegeln",
        "flip_v": "Vertikal spiegeln",
        "crop": "Als AVIF zuschneiden",
        "copy_path": "Pfad kopieren",
        "copy_image": "Bild kopieren",
        "trash": "In den Papierkorb",
        "reveal": "Im Ordner zeigen",
        "color": "Farbe",
    },
}


def language() -> str:
    """Return the active tools-window language code."""
    code = os.environ.get("LUCENT_LANG", "en").lower()[:2]
    if code in _TEXT:
        return code
    return "en"


def label(key: str) -> str:
    """Return the translated label for `key`."""
    return _TEXT[language()][key]
