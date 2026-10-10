// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>

namespace lucent {

/// Phrase shown in the viewer. The active language supplies the words.
enum class Text {
    Fit,              ///< Fit the image inside the window.
    Fill,             ///< Crop the image so it covers the window.
    OneToOne,         ///< Show one image pixel as one screen pixel.
    FitWidth,         ///< Fit the image to the window width.
    FitHeight,        ///< Fit the image to the window height.
    Free,             ///< Zoom freely.
    Slideshow,        ///< Start or stop the slideshow.
    HeifMissing,      ///< HEIF needs the system codec.
    PluginMissing,    ///< RAW, EXR, or HDR needs a system plugin.
    DecodeFailed,     ///< This image could not be decoded.
    Frame,            ///< Frame counter label.
    Tools,            ///< Open the tools window.
    Hotkeys,          ///< Open the hotkey editor.
    PressKey,         ///< Prompt while a new hotkey is captured.
    Language,         ///< Switch the interface language.
    Next,             ///< Next file in the folder.
    Previous,         ///< Previous file in the folder.
    Fullscreen,       ///< Enter or leave fullscreen.
    Frameless,        ///< Hide or show the window frame.
    WindowFit,        ///< Resize the window to the image.
    Quit,             ///< Quit the viewer.
    Open,             ///< Open a file.
    RotateLeft,       ///< Rotate a quarter turn left.
    RotateRight,      ///< Rotate a quarter turn right.
    FlipHorizontal,   ///< Flip left to right.
    FlipVertical,     ///< Flip top to bottom.
    Crop,             ///< Crop the image.
    Save,             ///< Save the image.
    SaveCopy,         ///< Save a copy.
    SaveAs,           ///< Save as a new file.
    Export,           ///< Export the current frame.
    Download,         ///< Save a copy into the downloads folder.
    MoveToTrash,      ///< Move the file to the trash.
    Enlarge,          ///< Hand the image to Enlarge.
    InstallEnlarge,   ///< Offer the Enlarge download page.
    EnlargeMissing,   ///< Enlarge is not installed.
    Settings,         ///< Open settings.
    About,            ///< Show version and licence.
    CopyImage,        ///< Copy the image to the clipboard.
    CopyPath,         ///< Copy the file path to the clipboard.
    RevealInFolder,   ///< Show the file in the file manager.
    Gallery,          ///< Show or hide the thumbnail strip.
    WelcomeImage,     ///< Welcome picture shown on the first empty launches.
    AppMenu,          ///< Application menu.
    SaveFailed,       ///< The save did not complete.
    TrashFailed,      ///< The file could not be moved to the trash.
};

/// Switch the language used by text(). `en`, `ru`, and `de` are recognised.
void setLanguage(const QString &code);

/// Return the active language code.
QString language();

/// Return the phrase for `id` in the active language.
QString text(Text id);

}  // namespace lucent
