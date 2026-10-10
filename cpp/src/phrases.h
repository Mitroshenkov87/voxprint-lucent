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
};

void setLanguage(const QString &code);
QString language();
QString text(Text id);

}  // namespace lucent
