// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QKeyEvent>
#include <QString>

#include <QWidget>

namespace lucent {

enum class Action {
    Next,
    Previous,
    Fit,
    Fill,
    OneToOne,
    FitWidth,
    FitHeight,
    Fullscreen,
    Frameless,
    WindowFit,
    Slideshow,
    NextFrame,
    PreviousFrame,
    Tools,
    Hotkeys,
    Language,
    Quit,
    Open,
    RotateLeft,
    RotateRight,
    FlipHorizontal,
    FlipVertical,
    Crop,
    SaveCopy,
    SaveAs,
    Trash,
    Enlarge,
    Settings,
    CopyImage,
    CopyPath,
    Reveal,
    Gallery,
};

void loadHotkeys();
Action actionFor(const QKeyEvent *event, bool *found);
QString actionLabel(Action action);
void editHotkeys(QWidget *parent);

}  // namespace lucent
