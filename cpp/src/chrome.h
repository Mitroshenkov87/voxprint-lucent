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

struct ChromeCallbacks {
    std::function<void(const QString &)> openPath;
    std::function<void(int)> step;
    std::function<void(int)> openIndex;
    std::function<void(ZoomMode)> zoom;
    std::function<void(bool right)> rotate;
    std::function<void(bool horizontal)> flip;
    std::function<void()> crop;
    std::function<void()> save;
    std::function<void()> saveCopy;
    std::function<void()> saveAs;
    std::function<void()> exportImage;
    std::function<void()> download;
    std::function<void()> trash;
    std::function<void()> slideshow;
    std::function<void()> fullscreen;
    std::function<void()> enlarge;
    std::function<void()> hotkeys;
    std::function<void(const QString &)> language;
    std::function<void(bool)> galleryVisible;
    std::function<void(int)> slideshowMs;
    std::function<QImage()> image;
    std::function<QString()> path;
};

// Toolbar, menus, settings, and the thumbnail strip. Construct this only after
// the first image paint.
class Chrome {
public:
    Chrome(QWidget *host, ChromeCallbacks callbacks);
    ~Chrome();

    void relayout();
    void mouseAt(const QPoint &pos);
    void setFolder(const QStringList &files, int index);
    void setCurrent(int index);
    void contextMenu(const QPoint &globalPos);
    int galleryHeight() const;
    void prepareShots();
    void showSettings();
    void toggleGallery();
    void copyImage();
    void copyPath();
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

void requestAcrylic(QWidget *window);

}  // namespace lucent
