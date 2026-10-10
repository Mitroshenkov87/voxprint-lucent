// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QIcon>

namespace lucent {

/// Toolbar and menu icon drawn by the viewer.
enum class Icon {
    Menu,             ///< Application menu.
    Open,             ///< Open a file.
    Previous,         ///< Previous file.
    Next,             ///< Next file.
    Fit,              ///< Fit inside the window.
    Fill,             ///< Cover the window.
    OneToOne,         ///< One image pixel per screen pixel.
    FitWidth,         ///< Fit to the width.
    FitHeight,        ///< Fit to the height.
    RotateLeft,       ///< Quarter turn left.
    RotateRight,      ///< Quarter turn right.
    FlipHorizontal,   ///< Flip left to right.
    FlipVertical,     ///< Flip top to bottom.
    Crop,             ///< Crop.
    Save,             ///< Save.
    SaveAs,           ///< Save as.
    SaveCopy,         ///< Save a copy.
    Export,           ///< Export.
    Download,         ///< Download a copy.
    Trash,            ///< Move to trash.
    Slideshow,        ///< Slideshow.
    Fullscreen,       ///< Fullscreen.
    Gallery,          ///< Thumbnail strip.
    Enlarge,          ///< Enlarge.
    Settings,         ///< Settings.
    Copy,             ///< Copy the image.
    Path,             ///< Copy the path.
    Reveal,           ///< Reveal in the file manager.
};

/// Return the icon for `id`, tinted for the current palette.
QIcon lucentIcon(Icon id);

}  // namespace lucent
