// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QIcon>

namespace lucent {

enum class Icon {
    Menu,
    Open,
    Previous,
    Next,
    Fit,
    Fill,
    OneToOne,
    FitWidth,
    FitHeight,
    RotateLeft,
    RotateRight,
    FlipHorizontal,
    FlipVertical,
    Crop,
    Save,
    SaveAs,
    Trash,
    Slideshow,
    Fullscreen,
    Gallery,
    Enlarge,
    Settings,
    Copy,
    Path,
    Reveal,
};

QIcon lucentIcon(Icon id);

}  // namespace lucent
