// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QKeyEvent>
#include <QString>

#include <QWidget>

namespace lucent {

/// Viewer action that a key can invoke.
enum class Action {
    Next,              ///< Next file.
    Previous,          ///< Previous file.
    Fit,               ///< Fit inside the window.
    Fill,              ///< Cover the window.
    OneToOne,          ///< One image pixel per screen pixel.
    FitWidth,          ///< Fit to the width.
    FitHeight,         ///< Fit to the height.
    Fullscreen,        ///< Fullscreen.
    Frameless,         ///< Frameless window.
    WindowFit,         ///< Resize the window to the image.
    Slideshow,         ///< Slideshow.
    NextFrame,         ///< Next frame of this file.
    PreviousFrame,     ///< Previous frame of this file.
    Tools,             ///< Tools window.
    Hotkeys,           ///< Hotkey editor.
    Language,          ///< Language.
    Quit,              ///< Quit.
    Open,              ///< Open a file.
    RotateLeft,        ///< Quarter turn left.
    RotateRight,       ///< Quarter turn right.
    FlipHorizontal,    ///< Flip left to right.
    FlipVertical,      ///< Flip top to bottom.
    Crop,              ///< Crop.
    SaveCopy,          ///< Save a copy.
    SaveAs,            ///< Save as.
    Trash,             ///< Move to trash.
    Enlarge,           ///< Enlarge.
    Settings,          ///< Settings.
    CopyImage,         ///< Copy the image.
    CopyPath,          ///< Copy the path.
    Reveal,            ///< Reveal in the file manager.
    Gallery,           ///< Thumbnail strip.
};

/// Load saved key bindings, or the defaults when none are saved.
void loadHotkeys();

/// Return the action for `event`. `found` is set when a binding matches.
Action actionFor(const QKeyEvent *event, bool *found);

/// Return the short name of `action`, for the hotkey editor.
QString actionLabel(Action action);

/// Open the dialog that rebinds keys. `parent` owns the dialog.
void editHotkeys(QWidget *parent);

}  // namespace lucent
