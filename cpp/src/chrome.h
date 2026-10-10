// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "placement.h"

#include "welcome_policy.h"

#include <QImage>
#include <QPoint>
#include <QString>
#include <QStringList>

#include <functional>

class QWidget;

namespace lucent {

/// Callbacks the toolbar uses to reach the viewer.
struct ChromeCallbacks {
    /// Open this path.
    std::function<void(const QString &)> openPath;
    /// Move this many files forward (negative moves backward).
    std::function<void(int)> step;
    /// Open the folder entry at this index.
    std::function<void(int)> openIndex;
    /// Switch fit mode.
    std::function<void(ZoomMode)> zoom;
    /// Rotate. `right` is clockwise.
    std::function<void(bool right)> rotate;
    /// Flip. `horizontal` flips left to right.
    std::function<void(bool horizontal)> flip;
    /// Start or apply a crop.
    std::function<void()> crop;
    /// Save the current image.
    std::function<void()> save;
    /// Save a copy beside the file.
    std::function<void()> saveCopy;
    /// Save as a new path.
    std::function<void()> saveAs;
    /// Export the current frame.
    std::function<void()> exportImage;
    /// Save a copy into the downloads folder.
    std::function<void()> download;
    /// Move the file to the trash.
    std::function<void()> trash;
    /// Start or stop the slideshow.
    std::function<void()> slideshow;
    /// Enter or leave fullscreen.
    std::function<void()> fullscreen;
    /// Hand the image to Enlarge.
    std::function<void()> enlarge;
    /// Open the hotkey editor.
    std::function<void()> hotkeys;
    /// Switch to this language code.
    std::function<void(const QString &)> language;
    /// Show or hide the thumbnail strip.
    std::function<void(bool)> galleryVisible;
    /// Set the slideshow delay in milliseconds.
    std::function<void(int)> slideshowMs;
    /// Current image, for copy and save.
    std::function<QImage()> image;
    /// Current file path.
    std::function<QString()> path;
};

/// Toolbar, menus, settings, and the thumbnail strip.
///
/// Construct this only after the first image paint.
class Chrome {
public:
    /// Build the toolbar on `host` and wire it to `callbacks`.
    Chrome(QWidget *host, ChromeCallbacks callbacks);
    /// Destroy the toolbar widgets.
    ~Chrome();

    /// Place the toolbar and thumbnail strip for the current window size.
    void relayout();
    /// Show or hide the toolbar based on `pos` inside the window.
    void mouseAt(const QPoint &pos);
    /// Point the thumbnail strip at `files`, highlighting `index`.
    void setFolder(const QStringList &files, int index);
    /// Highlight `index` in the thumbnail strip.
    void setCurrent(int index);
    /// Open the image context menu at `globalPos`.
    void contextMenu(const QPoint &globalPos);
    /// Height reserved for the thumbnail strip, or 0 when it is hidden.
    int galleryHeight() const;
    /// Decode thumbnails ahead of a screenshot or a test.
    void prepareShots();
    /// Open the settings dialog.
    void showSettings();
    /// Show or hide the thumbnail strip.
    void toggleGallery();
    /// Copy the current image to the clipboard.
    void copyImage();
    /// Copy the current path to the clipboard.
    void copyPath();
    /// Show the current file in the file manager.
    void reveal();

private:
    void openFile();
    void openMenu();
    void openSettings();
    void openAbout();
    void setGalleryVisible(bool visible);

    struct Impl;
    Impl *impl_;
};

/// Ask the platform for a translucent window background. No effect when unsupported.
void requestAcrylic(QWidget *window);

}  // namespace lucent
