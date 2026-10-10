// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>

namespace lucent {

enum class Text {
    Fit,
    Fill,
    OneToOne,
    FitWidth,
    FitHeight,
    Free,
    Slideshow,
    HeifMissing,
    PluginMissing,
    DecodeFailed,
    Frame,
    Tools,
    Hotkeys,
    PressKey,
    Language,
    Next,
    Previous,
    Fullscreen,
    Frameless,
    WindowFit,
    Quit,
    Open,
    RotateLeft,
    RotateRight,
    FlipHorizontal,
    FlipVertical,
    Crop,
    Save,
    SaveCopy,
    SaveAs,
    Export,
    Download,
    MoveToTrash,
    Enlarge,
    InstallEnlarge,
    EnlargeMissing,
    Settings,
    About,
    CopyImage,
    CopyPath,
    RevealInFolder,
    Gallery,
    WelcomeImage,
    AppMenu,
    SaveFailed,
    TrashFailed,
};

void setLanguage(const QString &code);
QString language();
QString text(Text id);

}  // namespace lucent
