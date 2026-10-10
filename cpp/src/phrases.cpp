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
    case Text::Open:
        return {"Open", "Открыть", "Öffnen"};
    case Text::RotateLeft:
        return {"Rotate left", "Повернуть влево", "Links drehen"};
    case Text::RotateRight:
        return {"Rotate right", "Повернуть вправо", "Rechts drehen"};
    case Text::FlipHorizontal:
        return {"Flip horizontal", "Отразить по горизонтали", "Horizontal spiegeln"};
    case Text::FlipVertical:
        return {"Flip vertical", "Отразить по вертикали", "Vertikal spiegeln"};
    case Text::Crop:
        return {"Crop", "Обрезать", "Zuschneiden"};
    case Text::Save:
        return {"Save", "Сохранить", "Speichern"};
    case Text::SaveCopy:
        return {"Save a copy", "Сохранить копию", "Kopie speichern"};
    case Text::SaveAs:
        return {"Save as", "Сохранить как", "Speichern unter"};
    case Text::Export:
        return {"Export", "Экспорт", "Exportieren"};
    case Text::Download:
        return {"Download", "Скачать", "Herunterladen"};
    case Text::MoveToTrash:
        return {"Move to trash", "В корзину", "In den Papierkorb"};
    case Text::Enlarge:
        return {"Enlarge", "Enlarge", "Enlarge"};
    case Text::InstallEnlarge:
        return {"Install Enlarge", "Установить Enlarge", "Enlarge installieren"};
    case Text::EnlargeMissing:
        return {"Enlarge is not installed.", "Enlarge не установлен.", "Enlarge ist nicht installiert."};
    case Text::Settings:
        return {"Settings", "Настройки", "Einstellungen"};
    case Text::About:
        return {"About", "О программе", "Info"};
    case Text::CopyImage:
        return {"Copy image", "Копировать изображение", "Bild kopieren"};
    case Text::CopyPath:
        return {"Copy path", "Копировать путь", "Pfad kopieren"};
    case Text::RevealInFolder:
        return {"Reveal in folder", "Показать в папке", "Im Ordner zeigen"};
    case Text::Gallery:
        return {"Thumbnails", "Миниатюры", "Miniaturen"};
    case Text::WelcomeImage:
        return {"Welcome image", "Приветственное изображение", "Willkommensbild"};
    case Text::AppMenu:
        return {"Menu", "Меню", "Menü"};
    case Text::SaveFailed:
        return {"Could not save that file.", "Не удалось сохранить файл.", "Die Datei konnte nicht gespeichert werden."};
    case Text::TrashFailed:
        return {"Could not move that file to the trash.", "Не удалось переместить файл в корзину.",
                "Die Datei konnte nicht in den Papierkorb verschoben werden."};
    }
    const Text missed = id;
    Q_UNUSED(missed);
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
