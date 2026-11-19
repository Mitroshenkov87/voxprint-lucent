// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "phrases.h"

namespace lucent {
namespace {

QString gLanguage = QStringLiteral("en");

struct Row {
    const char *en;
    const char *de;
};

Row rowFor(Text id) {
    switch (id) {
    case Text::Fit:
        return {"Fit", "Einpassen"};
    case Text::Fill:
        return {"Fill", "Füllen"};
    case Text::OneToOne:
        return {"1:1", "1:1"};
    case Text::FitWidth:
        return {"Fit width", "Breite"};
    case Text::FitHeight:
        return {"Fit height", "Höhe"};
    case Text::Free:
        return {"Zoom", "Zoom"};
    case Text::Slideshow:
        return {"Slideshow", "Diashow"};
    case Text::HeifMissing:
        return {"HEIF needs the system codec. libde265 and x265 are not bundled.", "HEIF braucht den System-Codec. libde265 und x265 werden nicht mitgeliefert."};
    case Text::PluginMissing:
        return {"This format needs a system image plugin (RAW, EXR, or HDR).", "Dieses Format braucht ein System-Plugin (RAW, EXR oder HDR)."};
    case Text::DecodeFailed:
        return {"Could not decode this image.", "Das Bild konnte nicht dekodiert werden."};
    case Text::Frame:
        return {"Frame", "Frame"};
    case Text::Tools:
        return {"Tools", "Werkzeuge"};
    case Text::Hotkeys:
        return {"Hotkeys", "Tasten"};
    case Text::PressKey:
        return {"Press the new key", "Neue Taste drücken"};
    case Text::Language:
        return {"Language", "Sprache"};
    case Text::Next:
        return {"Next", "Weiter"};
    case Text::Previous:
        return {"Previous", "Zurück"};
    case Text::Fullscreen:
        return {"Fullscreen", "Vollbild"};
    case Text::Frameless:
        return {"Frameless", "Rahmenlos"};
    case Text::WindowFit:
        return {"Window fit", "Fenster anpassen"};
    case Text::Quit:
        return {"Quit", "Beenden"};
    case Text::Open:
        return {"Open", "Öffnen"};
    case Text::RotateLeft:
        return {"Rotate left", "Links drehen"};
    case Text::RotateRight:
        return {"Rotate right", "Rechts drehen"};
    case Text::FlipHorizontal:
        return {"Flip horizontal", "Horizontal spiegeln"};
    case Text::FlipVertical:
        return {"Flip vertical", "Vertikal spiegeln"};
    case Text::Crop:
        return {"Crop", "Zuschneiden"};
    case Text::Save:
        return {"Save", "Speichern"};
    case Text::SaveCopy:
        return {"Save a copy", "Kopie speichern"};
    case Text::SaveAs:
        return {"Save as", "Speichern unter"};
    case Text::Export:
        return {"Export", "Exportieren"};
    case Text::Download:
        return {"Download", "Herunterladen"};
    case Text::MoveToTrash:
        return {"Move to trash", "In den Papierkorb"};
    case Text::Enlarge:
        return {"Enlarge", "Enlarge"};
    case Text::InstallEnlarge:
        return {"Install Enlarge", "Enlarge installieren"};
    case Text::EnlargeMissing:
        return {"Enlarge is not installed.", "Enlarge ist nicht installiert."};
    case Text::Settings:
        return {"Settings", "Einstellungen"};
    case Text::About:
        return {"About", "Info"};
    case Text::CopyImage:
        return {"Copy image", "Bild kopieren"};
    case Text::CopyPath:
        return {"Copy path", "Pfad kopieren"};
    case Text::RevealInFolder:
        return {"Reveal in folder", "Im Ordner zeigen"};
    case Text::Gallery:
        return {"Thumbnails", "Miniaturen"};
    case Text::WelcomeImage:
        return {"Welcome image", "Willkommensbild"};
    case Text::AppMenu:
        return {"Menu", "Menü"};
    case Text::SaveFailed:
        return {"Could not save that file.", "Die Datei konnte nicht gespeichert werden."};
    case Text::TrashFailed:
        return {"Could not move that file to the trash.", "Die Datei konnte nicht in den Papierkorb verschoben werden."};
    }
    const Text missed = id;
    Q_UNUSED(missed);
    return {"", ""};
}

}  // namespace

void setLanguage(const QString &code) {
    const QString lower = code.trimmed().toLower().left(2);
    if (lower == QStringLiteral("de") || lower == QStringLiteral("uk") || lower == QStringLiteral("lv")) {
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
    if (gLanguage == QStringLiteral("de")) {
        return QString::fromUtf8(row.de);
    }
    return QString::fromUtf8(row.en);
}

}  // namespace lucent
