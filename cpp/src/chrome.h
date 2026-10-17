// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "placement.h"
#include "shell_logic.h"

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <functional>

class QComboBox;
class QMenu;
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
    /// Apply a numeric crop in image pixels.
    std::function<void(int x, int y, int w, int h)> numericCrop;
    /// Step one frame. Negative moves backward.
    std::function<void(int)> stepFrame;
    /// Start or stop frame playback.
    std::function<void()> togglePlay;
    /// Set a free zoom factor in view pixels per image pixel.
    std::function<void(double)> setScale;
    /// Read this repository's releases. Not on the cold-start path.
    std::function<void()> checkUpdate;
    /// Hide or show the window frame.
    std::function<void()> frameless;
    /// Resize the window to the picture.
    std::function<void()> windowFit;
    /// Open the tools window.
    std::function<void()> tools;
    /// Replace the picture with this pixel size. `smooth` picks the resampling.
    std::function<void(int width, int height, bool smooth)> resizeImage;
};

/// Values the expert panel shows for the open picture.
struct LiveView {
    /// View pixels per image pixel.
    double scale = 1.0;
    /// Current zoom mode.
    ZoomMode mode = ZoomMode::Fit;
    /// Crop origin and size in image pixels. Zero size means the whole picture.
    int cropX = 0;
    int cropY = 0;
    int cropW = 0;
    int cropH = 0;
    /// Quarter-turns clockwise from the file, 0–270.
    int rotation = 0;
    /// Zero-based frame index.
    int frame = 0;
    /// Frame count. 1 for a still picture.
    int frameCount = 1;
    /// Picture width in pixels.
    int imageW = 0;
    /// Picture height in pixels.
    int imageH = 0;
    /// One-line file facts.
    QString info;
    /// Colour profile name, or empty.
    QString profile;
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
    /// Height reserved for the thumbnail strip, or 0 when it is hidden or collapsed.
    int galleryHeight() const;
    /// Rectangle the picture occupies. The expert panel is not included.
    QRect resultRect() const;
    /// Push live zoom, crop, frames, and info into the expert panel.
    void setLive(const LiveView &live);
    /// A zoom change that did not come from a preset chip.
    void noteZoom(ZoomMode mode, double scale);
    /// Reload catalog strings.
    void retranslate();
    /// Write the host geometry into the app settings file.
    void saveGeometry();
    /// Export suffix from the expert panel: avif, jxl, or webp.
    QString exportSuffix() const;
    /// Export quality from the expert panel, 1–100.
    int exportQuality() const;
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
    void loadPresets();
    void persistState();
    void persistFlag(const std::string &key, const Json &value);
    bool panelOpen() const;
    void buildBar();
    void buildPresets();
    void buildExpert();
    void buildStatus();
    void buildCompare();
    void refreshChips();
    void refreshExpert();
    void chooseChip(const QString &id);
    void resetView();
    void applyZoom();
    void commitZoomSpin();
    void commitCombo(QComboBox *box, const char *key);
    void resizeExpert(int delta);
    void startJob();
    void readJob();
    void finishJob(int code);
    void cancelJob();
    void setExpert(bool enabled);
    void openFile();
    void openMenu();
    void addKeptActions(QMenu &menu);
    void openResizeDialog();
    void editResize(const ResizeSpec &spec);
    void syncResizeFields();
    void applyResize();
    void openSettings();
    void openAbout();
    void setGalleryVisible(bool visible);

    struct Impl;
    Impl *impl_;
};

/// Ask the platform for a translucent window background. No effect when unsupported.
void requestAcrylic(QWidget *window);

}  // namespace lucent
