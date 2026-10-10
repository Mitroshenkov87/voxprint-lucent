// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "browser.h"

#include "catalog.h"
#include "decode.h"
#include "hotkeys.h"
#include "imagecache.h"
#include "rhi_view.h"
#include "phrases.h"
#include "theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>

#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>

namespace lucent {
namespace {

bool isRawOrHdr(const QString &suffix) {
    return suffix == QStringLiteral("exr") || suffix == QStringLiteral("hdr") || suffix == QStringLiteral("raw")
        || suffix == QStringLiteral("cr2") || suffix == QStringLiteral("cr3") || suffix == QStringLiteral("nef")
        || suffix == QStringLiteral("arw") || suffix == QStringLiteral("dng") || suffix == QStringLiteral("orf")
        || suffix == QStringLiteral("rw2") || suffix == QStringLiteral("raf") || suffix == QStringLiteral("pef");
}

QString failureText(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("heic") || suffix == QStringLiteral("heif")) {
        return text(Text::HeifMissing);
    }
    if (isRawOrHdr(suffix)) {
        return text(Text::PluginMissing);
    }
    return text(Text::DecodeFailed);
}

}  // namespace

void deliver(const std::shared_ptr<std::atomic<bool>> &alive, const QPointer<Browser> &self, std::function<void(Browser *)> fn) {
    if (!alive->load()) {
        return;
    }
    QCoreApplication *app = QCoreApplication::instance();
    if (app == nullptr) {
        return;
    }
    QMetaObject::invokeMethod(
        app,
        [alive, self, fn = std::move(fn)] {
            if (!alive->load() || self.isNull()) {
                return;
            }
            fn(self.data());
        },
        Qt::QueuedConnection);
}

Browser::Browser(LaunchOptions options, std::chrono::steady_clock::time_point started, QWidget *parent)
    : QWidget(parent), options_(std::move(options)) {
    Q_UNUSED(started);
    setObjectName(QStringLiteral("lucent-window"));
    setWindowTitle(QStringLiteral("Voxprint AI Lucent"));
    setStyleSheet(loadStyleSheet());
    resize(1600, 1000);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    cache_ = new PreviewCache();
    openPath(options_.image);
}

Browser::~Browser() {
    alive_->store(false);
    ++requestId_;
    delete cache_;
}

void Browser::openPath(const QString &path) {
    const QFileInfo info(path);
    if (info.isDir()) {
        const QStringList files = listFolderImages(info.absoluteFilePath());
        if (files.isEmpty()) {
            error_ = text(Text::DecodeFailed);
            update();
            return;
        }
        folder_ = files;
        index_ = 0;
        openFile(files.first());
        return;
    }
    openFile(info.absoluteFilePath());
}

void Browser::openFile(const QString &absolutePath) {
    currentPath_ = absolutePath;
    frame_ = 0;
    frameCount_ = 1;
    error_.clear();
    if (animation_ != nullptr) {
        animation_->stop();
    }
    setWindowTitle(QFileInfo(absolutePath).fileName());
    const int request = ++requestId_;
    const QString key = cacheKey(absolutePath, 0, false);
    const QImage cached = cache_->get(key);
    if (!cached.isNull()) {
        showImage(cached, true);
        return;
    }
    const QSize box = size();
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread worker([absolutePath, box, request, self, alive] {
        const QImage decoded = decodePreview(absolutePath, box);
        deliver(alive, self, [absolutePath, decoded, request](Browser *browser) {
            if (request != browser->requestId_ || absolutePath != browser->currentPath_) {
                return;
            }
            browser->cache_->put(browser->cacheKey(absolutePath, 0, false), decoded);
            browser->showImage(decoded, true);
        });
    });
    worker.detach();
}

void Browser::showImage(const QImage &image, bool resetZoom) {
    image_ = image;
    if (image.isNull()) {
        error_ = failureText(currentPath_);
    } else {
        error_.clear();
        if (resetZoom) {
            mode_ = ZoomMode::Fit;
            placement_ = placementFor(mode_, image.width(), image.height(), width(), height());
        }
    }
    syncRhi();
    updateChrome();
    update();
    if (resetZoom) {
        noteFrames(currentPath_);
    }
    if (!armed_) {
        armed_ = true;
        QTimer::singleShot(0, this, [this] { armAfterFirstPaint(); });
    }
}

void Browser::armAfterFirstPaint() {
    const QByteArray env = qgetenv("LUCENT_LANG");
    if (!env.isEmpty()) {
        setLanguage(QString::fromUtf8(env));
    } else {
        const QString locale = QLocale::system().name().left(2).toLower();
        setLanguage(locale);
        QSettings settings;
        if (settings.contains(QStringLiteral("language"))) {
            setLanguage(settings.value(QStringLiteral("language")).toString());
        }
    }
    loadHotkeys();
    status_ = new QLabel(this);
    status_->setStyleSheet(QStringLiteral("color: #f2f2f5; background: transparent;"));
    status_->setAttribute(Qt::WA_TransparentForMouseEvents);
    pool_ = new QThreadPool(this);
    pool_->setMaxThreadCount(2);
    animation_ = new QTimer(this);
    connect(animation_, &QTimer::timeout, this, [this] { stepFrame(1); });
    slideshow_ = new QTimer(this);
    slideshow_->setInterval(3000);
    connect(slideshow_, &QTimer::timeout, this, [this] { stepFile(1); });
    inertia_ = new QTimer(this);
    inertia_->setInterval(16);
    connect(inertia_, &QTimer::timeout, this, [this] {
        placement_ = panBy(placement_, velocity_.x(), velocity_.y());
        velocity_ *= 0.88;
        syncRhi();
        update();
        if (std::hypot(velocity_.x(), velocity_.y()) < 0.6) {
            inertia_->stop();
        }
    });

    const QString path = currentPath_;
    const QString directory = QFileInfo(path).absolutePath();
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread folderWorker([directory, path, self, alive] {
        const QStringList files = listFolderImages(directory);
        deliver(alive, self, [files, directory, path](Browser *browser) {
            if (QFileInfo(browser->currentPath_).absolutePath() != directory) {
                return;
            }
            browser->folder_ = files;
            browser->index_ = browser->folder_.indexOf(path);
            browser->scheduleLookahead();
        });
    });
    folderWorker.detach();

    std::thread frameWorker([path, self, alive] {
        const int count = frameCount(path);
        const int delay = frameDelayMs(path, 0);
        deliver(alive, self, [path, count, delay](Browser *browser) {
            if (path != browser->currentPath_ || browser->animation_ == nullptr) {
                return;
            }
            browser->frameCount_ = count;
            if (count > 1) {
                browser->animation_->setInterval(delay);
                browser->animation_->start();
            }
            browser->updateChrome();
        });
    });
    frameWorker.detach();

    rhi_ = RhiSurface::tryCreate(this);
    if (rhi_ != nullptr) {
        rhi_->setGeometry(rect());
        connect(rhi_, &RhiSurface::failed, this, [this] {
            if (rhi_ != nullptr) {
                rhi_->hide();
                update();
            }
        });
        syncRhi();
        rhi_->show();
    }
    updateChrome();
}

void Browser::applyMode(ZoomMode mode) {
    mode_ = mode;
    if (!image_.isNull() && mode != ZoomMode::Free) {
        placement_ = placementFor(mode, image_.width(), image_.height(), width(), height());
    }
    if (mode == ZoomMode::OneToOne) {
        ensureFullResolution();
    }
    syncRhi();
    updateChrome();
    update();
}

void Browser::updateChrome() {
    if (status_ == nullptr) {
        return;
    }
    QString line = error_;
    if (line.isEmpty()) {
        Text modeText = Text::Free;
        switch (mode_) {
        case ZoomMode::Fit:
            modeText = Text::Fit;
            break;
        case ZoomMode::Fill:
            modeText = Text::Fill;
            break;
        case ZoomMode::OneToOne:
            modeText = Text::OneToOne;
            break;
        case ZoomMode::FitWidth:
            modeText = Text::FitWidth;
            break;
        case ZoomMode::FitHeight:
            modeText = Text::FitHeight;
            break;
        case ZoomMode::Free:
            modeText = Text::Free;
            break;
        }
        line = QFileInfo(currentPath_).fileName() + QStringLiteral("  ") + text(modeText);
        if (frameCount_ > 1) {
            line += QStringLiteral("  ") + text(Text::Frame) + QStringLiteral(" ") + QString::number(frame_ + 1)
                + QStringLiteral("/") + QString::number(frameCount_);
        }
        if (slideshowOn_) {
            line += QStringLiteral("  ") + text(Text::Slideshow);
        }
    }
    status_->setText(line);
    status_->adjustSize();
    status_->move(12, height() - status_->height() - 12);
    status_->raise();
}

void Browser::noteFrames(const QString &path) {
    if (animation_ == nullptr || path.isEmpty()) {
        return;
    }
    animation_->stop();
    frameCount_ = 1;
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread worker([path, self, alive] {
        const int count = frameCount(path);
        const int delay = frameDelayMs(path, 0);
        deliver(alive, self, [path, count, delay](Browser *browser) {
            if (path != browser->currentPath_ || browser->animation_ == nullptr) {
                return;
            }
            browser->frameCount_ = count;
            if (count > 1) {
                browser->animation_->setInterval(std::max(delay, 20));
                browser->animation_->start();
            }
            browser->updateChrome();
        });
    });
    worker.detach();
}

void Browser::scheduleLookahead() {
    if (pool_ == nullptr || folder_.isEmpty() || index_ < 0) {
        return;
    }
    const QSize box = size();
    for (const int delta : {-2, -1, 1, 2}) {
        const int neighbor = index_ + delta;
        if (neighbor < 0 || neighbor >= folder_.size()) {
            continue;
        }
        const QString path = folder_.at(neighbor);
        const QString key = cacheKey(path, 0, false);
        if (!cache_->get(key).isNull()) {
            continue;
        }
        auto *watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key] {
            cache_->put(key, watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run(pool_, [path, box] { return decodePreview(path, box); }));
    }
}

void Browser::stepFile(int delta) {
    if (folder_.isEmpty() || index_ < 0) {
        return;
    }
    int next = index_ + delta;
    if (next < 0) {
        next = folder_.size() - 1;
    }
    if (next >= folder_.size()) {
        next = 0;
    }
    index_ = next;
    openFile(folder_.at(index_));
    scheduleLookahead();
}

void Browser::stepFrame(int delta) {
    if (frameCount_ <= 1 || currentPath_.isEmpty()) {
        return;
    }
    int next = frame_ + delta;
    if (next < 0) {
        next = frameCount_ - 1;
    }
    if (next >= frameCount_) {
        next = 0;
    }
    frame_ = next;
    const int request = ++requestId_;
    const QString path = currentPath_;
    const QString key = cacheKey(path, frame_, false);
    const QImage cached = cache_->get(key);
    if (!cached.isNull()) {
        showImage(cached, false);
        return;
    }
    const QSize box = size();
    const int frame = frame_;
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread worker([path, box, frame, request, self, alive] {
        const QImage decoded = decodeFrame(path, frame, box);
        deliver(alive, self, [path, decoded, frame, request](Browser *browser) {
            if (request != browser->requestId_ || path != browser->currentPath_) {
                return;
            }
            browser->frame_ = frame;
            browser->cache_->put(browser->cacheKey(path, frame, false), decoded);
            browser->showImage(decoded, false);
        });
    });
    worker.detach();
}

void Browser::ensureFullResolution() {
    if (image_.isNull() || currentPath_.isEmpty() || placement_.scale <= 1.05) {
        return;
    }
    const QString path = currentPath_;
    const int frame = frame_;
    const QString key = cacheKey(path, frame, true);
    const QImage cached = cache_->get(key);
    if (!cached.isNull()) {
        showImage(cached, false);
        return;
    }
    const int request = requestId_;
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread worker([path, frame, request, self, alive] {
        const QImage decoded = decodeFrame(path, frame, QSize());
        deliver(alive, self, [path, decoded, frame, request](Browser *browser) {
            if (request != browser->requestId_ || path != browser->currentPath_ || frame != browser->frame_) {
                return;
            }
            browser->cache_->put(browser->cacheKey(path, frame, true), decoded);
            browser->showImage(decoded, false);
        });
    });
    worker.detach();
}

void Browser::toggleSlideshow() {
    slideshowOn_ = !slideshowOn_;
    if (slideshow_ == nullptr) {
        return;
    }
    if (slideshowOn_) {
        slideshow_->start();
    } else {
        slideshow_->stop();
    }
    updateChrome();
}

void Browser::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void Browser::toggleFrameless() {
    const bool enable = (windowFlags() & Qt::FramelessWindowHint) == 0;
    setWindowFlag(Qt::FramelessWindowHint, enable);
    show();
}

void Browser::fitWindow() {
    if (image_.isNull()) {
        return;
    }
    const QSize wanted(qRound(image_.width() * placement_.scale), qRound(image_.height() * placement_.scale));
    QSize limit = wanted;
    if (screen() != nullptr) {
        limit = wanted.boundedTo(screen()->availableGeometry().size() - QSize(48, 80));
    }
    if (!isFullScreen()) {
        resize(limit.expandedTo(QSize(320, 200)));
    }
}

void Browser::openTools() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QString python = qEnvironmentVariable("LUCENT_PYTHON");
    const QString bundled = appDir + QStringLiteral("/python/python.exe");
    if (python.isEmpty() && QFileInfo::exists(bundled)) {
        python = bundled;
    }
    if (python.isEmpty()) {
#ifdef Q_OS_WIN
        python = QStringLiteral("python");
#else
        python = QStringLiteral("python3");
#endif
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList pythonPath;
    if (QFileInfo::exists(appDir + QStringLiteral("/voxprint_lucent/__init__.py"))) {
        pythonPath.append(appDir);
    }
    const QString debRoot = QDir(appDir).filePath(QStringLiteral("../lib/voxprint-lucent"));
    if (QFileInfo::exists(debRoot + QStringLiteral("/voxprint_lucent/__init__.py"))) {
        pythonPath.append(QDir(debRoot).absolutePath());
    }
    if (!pythonPath.isEmpty()) {
        const QString existing = env.value(QStringLiteral("PYTHONPATH"));
        if (!existing.isEmpty()) {
            pythonPath.append(existing);
        }
#ifdef Q_OS_WIN
        env.insert(QStringLiteral("PYTHONPATH"), pythonPath.join(QLatin1Char(';')));
#else
        env.insert(QStringLiteral("PYTHONPATH"), pythonPath.join(QLatin1Char(':')));
#endif
    }
    QProcess process;
    process.setProgram(python);
    process.setArguments({QStringLiteral("-m"), QStringLiteral("voxprint_lucent.tools_host"),
                          QStringLiteral("--path"), currentPath_});
    process.setWorkingDirectory(appDir);
    process.setProcessEnvironment(env);
    process.startDetached();
}

void Browser::cycleLanguage() {
    const QString current = language();
    if (current == QStringLiteral("en")) {
        setLanguage(QStringLiteral("ru"));
    } else if (current == QStringLiteral("ru")) {
        setLanguage(QStringLiteral("de"));
    } else {
        setLanguage(QStringLiteral("en"));
    }
    QSettings settings;
    settings.setValue(QStringLiteral("language"), language());
    updateChrome();
}

void Browser::syncRhi() {
    if (rhi_ == nullptr || image_.isNull()) {
        return;
    }
    rhi_->setImage(image_);
    rhi_->setPlacement(placement_);
}

QString Browser::cacheKey(const QString &path, int frame, bool full) const {
    return path + QStringLiteral("|") + QString::number(frame) + (full ? QStringLiteral("|full") : QStringLiteral("|preview"));
}

void Browser::paintEvent(QPaintEvent *) {
    if (rhi_ != nullptr && rhi_->isVisible()) {
        return;
    }
    QPainter painter(this);
    painter.fillRect(rect(), rootPlain());
    if (!image_.isNull()) {
        painter.save();
        painter.translate(-placement_.originX * placement_.scale, -placement_.originY * placement_.scale);
        painter.scale(placement_.scale, placement_.scale);
        painter.drawImage(QPointF(0, 0), image_);
        painter.restore();
    } else if (!error_.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#f2f2f5")));
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, error_);
    }
}

void Browser::keyPressEvent(QKeyEvent *event) {
    if (!armed_) {
        loadHotkeys();
    }
    const Qt::KeyboardModifiers mods = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (event->key() == Qt::Key_Escape && mods == Qt::NoModifier) {
        if (isFullScreen()) {
            showNormal();
            return;
        }
        if (slideshowOn_) {
            toggleSlideshow();
            return;
        }
    }
    bool found = false;
    const Action action = actionFor(event, &found);
    if (!found) {
        QWidget::keyPressEvent(event);
        return;
    }
    switch (action) {
    case Action::Next:
        stepFile(1);
        break;
    case Action::Previous:
        stepFile(-1);
        break;
    case Action::Fit:
        applyMode(ZoomMode::Fit);
        break;
    case Action::Fill:
        applyMode(ZoomMode::Fill);
        break;
    case Action::OneToOne:
        applyMode(ZoomMode::OneToOne);
        break;
    case Action::FitWidth:
        applyMode(ZoomMode::FitWidth);
        break;
    case Action::FitHeight:
        applyMode(ZoomMode::FitHeight);
        break;
    case Action::Fullscreen:
        toggleFullscreen();
        break;
    case Action::Frameless:
        toggleFrameless();
        break;
    case Action::WindowFit:
        fitWindow();
        break;
    case Action::Slideshow:
        toggleSlideshow();
        break;
    case Action::NextFrame:
        if (animation_ != nullptr) {
            animation_->stop();
        }
        stepFrame(1);
        break;
    case Action::PreviousFrame:
        if (animation_ != nullptr) {
            animation_->stop();
        }
        stepFrame(-1);
        break;
    case Action::Tools:
        openTools();
        break;
    case Action::Hotkeys:
        editHotkeys(this);
        break;
    case Action::Language:
        cycleLanguage();
        break;
    case Action::Quit:
        close();
        break;
    }
}

void Browser::wheelEvent(QWheelEvent *event) {
    if (image_.isNull()) {
        return;
    }
    const double steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) {
        return;
    }
    if (inertia_ != nullptr) {
        inertia_->stop();
    }
    mode_ = ZoomMode::Free;
    placement_ = zoomAtCursor(placement_, std::pow(1.1, steps), event->position().x(), event->position().y());
    ensureFullResolution();
    syncRhi();
    updateChrome();
    update();
}

void Browser::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = true;
    lastMouse_ = event->position();
    velocity_ = {};
    if (inertia_ != nullptr) {
        inertia_->stop();
    }
}

void Browser::mouseMoveEvent(QMouseEvent *event) {
    if (!dragging_) {
        return;
    }
    const QPointF delta = event->position() - lastMouse_;
    lastMouse_ = event->position();
    velocity_ = delta;
    mode_ = ZoomMode::Free;
    placement_ = panBy(placement_, delta.x(), delta.y());
    syncRhi();
    update();
}

void Browser::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = false;
    if (inertia_ != nullptr && std::hypot(velocity_.x(), velocity_.y()) > 2.0) {
        inertia_->start();
    }
}

void Browser::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (rhi_ != nullptr) {
        rhi_->setGeometry(rect());
    }
    if (!image_.isNull() && mode_ != ZoomMode::Free) {
        placement_ = placementFor(mode_, image_.width(), image_.height(), width(), height());
        syncRhi();
    }
    updateChrome();
}

}  // namespace lucent
