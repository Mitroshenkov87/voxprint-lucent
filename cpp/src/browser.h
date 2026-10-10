// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "placement.h"
#include "viewer.h"

#include <QImage>
#include <QPointer>
#include <QRect>
#include <QStringList>

#include <atomic>
#include <memory>

class QLabel;
class QThreadPool;
class QTimer;

namespace lucent {
class Chrome;
class SplashFrame;
}

namespace lucent {

class PreviewCache;
class RhiSurface;

// Interactive viewer. --benchmark does not construct this class, so the
// first-pixel measurement path stays in Viewer.
class Browser : public QWidget {
public:
    Browser(LaunchOptions options, std::chrono::steady_clock::time_point started, QWidget *parent = nullptr);
    ~Browser() override;

    void openPath(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void openFile(const QString &absolutePath, bool browse = true);
    void showImage(const QImage &image, bool resetZoom);
    void armAfterFirstPaint();
    void applyMode(ZoomMode mode);
    void updateChrome();
    void scheduleLookahead();
    void noteFrames(const QString &path);
    void stepFile(int delta);
    void stepFrame(int delta);
    void ensureFullResolution();
    void toggleSlideshow();
    void toggleFullscreen();
    void toggleFrameless();
    void fitWindow();
    void openTools(bool enlarge = false);
    void cycleLanguage();
    void ensureChrome();
    void refreshFolder();
    void offerSplash(const QImage &image);
    void startSplashLoad();
    void maybeWelcome();
    void rotateBy(bool right);
    void flipImage(bool horizontal);
    void beginCrop();
    void applyCrop();
    void cancelCrop();
    bool writeView(const QString &path);
    void saveCopy();
    void saveAs();
    void moveToTrash();
    void syncRhi();
    QString cacheKey(const QString &path, int frame, bool full) const;

    LaunchOptions options_;
    QString currentPath_;
    QStringList folder_;
    int index_ = -1;
    QImage image_;
    Placement placement_;
    ZoomMode mode_ = ZoomMode::Fit;
    int requestId_ = 0;
    bool armed_ = false;
    int frame_ = 0;
    int frameCount_ = 1;
    bool slideshowOn_ = false;
    bool dragging_ = false;
    QString error_;
    QPointF lastMouse_;
    QPointF velocity_;
    QLabel *status_ = nullptr;
    QTimer *animation_ = nullptr;
    QTimer *slideshow_ = nullptr;
    QTimer *inertia_ = nullptr;
    QThreadPool *pool_ = nullptr;
    RhiSurface *rhi_ = nullptr;
    PreviewCache *cache_ = nullptr;
    Chrome *chrome_ = nullptr;
    SplashFrame *splash_ = nullptr;
    bool imageReady_ = false;
    bool browseFolder_ = true;
    bool cropping_ = false;
    bool cropDragging_ = false;
    QRect cropRect_;
    QPoint cropAnchor_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
};

}  // namespace lucent
