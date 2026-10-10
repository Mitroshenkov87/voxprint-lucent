// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "phrases.h"

namespace lucent {
namespace {

QString gLanguage = QStringLiteral("en");

struct Row {
    const char *en;
    const char *ru;
    const char *de;
};

Row rowFor(Text id) {
    switch (id) {
    case Text::Fit:
        return {"Fit", "Вместить", "Einpassen"};
    case Text::Fill:
        return {"Fill", "Заполнить", "Füllen"};
    case Text::OneToOne:
        return {"1:1", "1:1", "1:1"};
    case Text::FitWidth:
        return {"Fit width", "По ширине", "Breite"};
    case Text::FitHeight:
        return {"Fit height", "По высоте", "Höhe"};
    case Text::Free:
        return {"Zoom", "Масштаб", "Zoom"};
    case Text::Slideshow:
        return {"Slideshow", "Слайд-шоу", "Diashow"};
    case Text::HeifMissing:
        return {"HEIF needs the system codec. libde265 and x265 are not bundled.",
                "Для HEIF нужен системный кодек. libde265 и x265 не входят в поставку.",
                "HEIF braucht den System-Codec. libde265 und x265 werden nicht mitgeliefert."};
    case Text::PluginMissing:
        return {"This format needs a system image plugin (RAW, EXR, or HDR).",
                "Для этого формата нужен системный плагин (RAW, EXR или HDR).",
                "Dieses Format braucht ein System-Plugin (RAW, EXR oder HDR)."};
    case Text::DecodeFailed:
        return {"Could not decode this image.", "Не удалось декодировать изображение.",
                "Das Bild konnte nicht dekodiert werden."};
    case Text::Frame:
        return {"Frame", "Кадр", "Frame"};
    case Text::Tools:
        return {"Tools", "Инструменты", "Werkzeuge"};
    case Text::Hotkeys:
        return {"Hotkeys", "Клавиши", "Tasten"};
    case Text::PressKey:
        return {"Press the new key", "Нажмите новую клавишу", "Neue Taste drücken"};
    case Text::Language:
        return {"Language", "Язык", "Sprache"};
    case Text::Next:
        return {"Next", "Следующее", "Weiter"};
    case Text::Previous:
        return {"Previous", "Предыдущее", "Zurück"};
    case Text::Fullscreen:
        return {"Fullscreen", "Полный экран", "Vollbild"};
    case Text::Frameless:
        return {"Frameless", "Без рамки", "Rahmenlos"};
    case Text::WindowFit:
        return {"Window fit", "Окно по картинке", "Fenster anpassen"};
    case Text::Quit:
        return {"Quit", "Выход", "Beenden"};
    }
    return {"", "", ""};
}

}  // namespace

void setLanguage(const QString &code) {
    const QString lower = code.trimmed().toLower().left(2);
    if (lower == QStringLiteral("ru") || lower == QStringLiteral("de")) {
        gLanguage = lower;
        return;
    }
    gLanguage = QStringLiteral("en");
}

QString language() {
    return gLanguage;
}

QString text(Text id) {
    const Row row = rowFor(id);
    if (gLanguage == QStringLiteral("ru")) {
        return QString::fromUtf8(row.ru);
    }
    if (gLanguage == QStringLiteral("de")) {
        return QString::fromUtf8(row.de);
    }
    return QString::fromUtf8(row.en);
}

}  // namespace lucent
