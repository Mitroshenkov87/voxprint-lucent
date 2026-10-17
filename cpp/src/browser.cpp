// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "browser.h"

#include "catalog.h"
#include "chrome.h"
#include "decode.h"
#include "dry_run.h"
#include "labels.h"
#include "shell_logic.h"
#include "hotkeys.h"
#include "imagecache.h"
#include "rhi_view.h"
#include "phrases.h"
#include "splash.h"
#include "theme.h"
#include "welcome_policy.h"

#include <QColorSpace>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageWriter>
#include <QTransform>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QStringList>
#include <QStandardPaths>
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

void restoreLaunchGeometry(QWidget *window) {
    window->setMinimumSize(640, 400);
    if (!qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        int width = 1200;
        int height = 900;
        const QString size = qEnvironmentVariable("LUCENT_SCREENSHOT_SIZE");
        const int split = size.indexOf(QLatin1Char('x'));
        if (split > 0) {
            const int requestedWidth = size.left(split).toInt();
            const int requestedHeight = size.mid(split + 1).toInt();
            if (requestedWidth >= 640 && requestedHeight >= 400) {
                width = requestedWidth;
                height = requestedHeight;
            }
        }
        window->resize(width, height);
        return;
    }
    const Json document = readDocument(appSettingsPath(kSurfaceId));
    const Json *geo = document.field("ui.geometry");
    const auto number = [](const Json *object, const char *key) -> int {
        if (object == nullptr) {
            return -1;
        }
        const Json *value = object->field(key);
        if (value == nullptr || !value->isNumber()) {
            return -1;
        }
        return static_cast<int>(std::lround(value->asNumber()));
    };
    const int width = number(geo, "width");
    const int height = number(geo, "height");
    if (geo == nullptr || geo->kind != Json::Kind::Object || width < 640 || height < 400) {
        window->resize(1200, 900);
        return;
    }
    const int x = number(geo, "x");
    const int y = number(geo, "y");
    if (x >= 0 && y >= 0) {
        window->setGeometry(x, y, width, height);
    } else {
        window->resize(width, height);
    }
    const Json *fullscreen = geo->field("fullscreen");
    const Json *maximized = geo->field("maximized");
    if (fullscreen != nullptr && fullscreen->kind == Json::Kind::Bool && fullscreen->boolean) {
        window->setWindowState(Qt::WindowFullScreen);
    } else if (maximized != nullptr && maximized->kind == Json::Kind::Bool && maximized->boolean) {
        window->setWindowState(Qt::WindowMaximized);
    }
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
    setObjectName(QStringLiteral("shell"));
    setWindowTitle(QStringLiteral("VOXPrint AI Lucent"));
    setStyleSheet(loadStyleSheet());
    restoreLaunchGeometry(this);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    cache_ = new PreviewCache();
    const QByteArray shot = qgetenv("LUCENT_SCREENSHOT");
    if (options_.image.isEmpty()) {
        if (shot != "splash") {
            maybeWelcome();
        }
    } else {
        openPath(options_.image);
    }
    startSplashLoad();
    if (currentPath_.isEmpty()) {
        QTimer::singleShot(0, this, [this] {
            if (!armed_) {
                armed_ = true;
                armAfterFirstPaint();
            }
        });
    }
}

Browser::~Browser() {
    alive_->store(false);
    ++requestId_;
    delete chrome_;
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

void Browser::openFile(const QString &absolutePath, bool browse) {
    browseFolder_ = browse;
    if (!browse) {
        folder_.clear();
        index_ = -1;
    }
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
        if (armed_) {
            refreshFolder();
        }
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
    if (armed_) {
        refreshFolder();
    }
}

void Browser::showImage(const QImage &image, bool resetZoom) {
    image_ = image;
    if (image.isNull()) {
        error_ = failureText(currentPath_);
    } else {
        error_.clear();
        if (resetZoom) {
            mode_ = ZoomMode::Fit;
            const QSize view = pictureRect().size();
            placement_ = placementFor(mode_, image.width(), image.height(), std::max(1, view.width()), std::max(1, view.height()));
        }
    }
    imageReady_ = !image.isNull();
    if (splash_ != nullptr && qgetenv("LUCENT_SCREENSHOT") != "splash") {
        splash_->hide();
        splash_->deleteLater();
        splash_ = nullptr;
    }
    syncRhi();
    updateChrome();
    update();
    if (chrome_ != nullptr) {
        chrome_->setCurrent(index_);
    }
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
    slideshow_->setInterval(QSettings().value(QStringLiteral("slideshow/ms"), 3000).toInt());
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

    refreshFolder();

    const QString path = currentPath_;
    const QPointer<Browser> self(this);
    const auto alive = alive_;
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
    ensureChrome();
}

QRect Browser::pictureRect() const {
    if (chrome_ == nullptr) {
        return rect();
    }
    const QRect view = chrome_->resultRect();
    return view.isEmpty() ? rect() : view;
}

void Browser::refit() {
    const QSize view = pictureRect().size();
    if (!image_.isNull() && mode_ != ZoomMode::Free && view.width() > 0 && view.height() > 0) {
        placement_ = placementFor(mode_, image_.width(), image_.height(), view.width(), view.height());
    }
    if (rhi_ != nullptr) {
        rhi_->setGeometry(pictureRect());
        rhi_->lower();
    }
    syncRhi();
}

QString Browser::exportSuffix() const {
    return chrome_ == nullptr ? QStringLiteral("avif") : chrome_->exportSuffix();
}

void Browser::applyMode(ZoomMode mode) {
    mode_ = mode;
    refit();
    if (mode == ZoomMode::OneToOne) {
        ensureFullResolution();
    }
    if (chrome_ != nullptr) {
        chrome_->noteZoom(mode_, placement_.scale);
    }
    updateChrome();
    update();
}

void Browser::updateChrome() {
    if (chrome_ != nullptr) {
        LiveView live;
        live.scale = placement_.scale;
        live.mode = mode_;
        live.frame = frame_;
        live.frameCount = frameCount_;
        if (!image_.isNull()) {
            live.imageW = image_.width();
            live.imageH = image_.height();
            const QFileInfo file(currentPath_);
            QStringList facts;
            facts << file.fileName();
            if (file.exists() && file.size() > 0) {
                facts << formatDataSize(file.size());
            }
            facts << QString::number(image_.width()) + QStringLiteral("×") + QString::number(image_.height());
            live.info = facts.join(QStringLiteral("  "));
            if (image_.colorSpace().isValid()) {
                live.profile = image_.colorSpace().description();
            }
            if (cropping_ && cropRect_.isValid()) {
                const QRect view = pictureRect();
                const QRect local = cropRect_.translated(-view.topLeft());
                live.cropX = qRound(local.x() / placement_.scale + placement_.originX);
                live.cropY = qRound(local.y() / placement_.scale + placement_.originY);
                live.cropW = qRound(local.width() / placement_.scale);
                live.cropH = qRound(local.height() / placement_.scale);
            }
        } else if (!error_.isEmpty()) {
            live.info = error_;
        }
        chrome_->setLive(live);
    }
    if (status_ == nullptr) {
        return;
    }
    status_->hide();
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
    const int strip = chrome_ == nullptr ? 0 : chrome_->galleryHeight();
    status_->move(12, height() - status_->height() - 12 - strip);
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
        resize(limit.expandedTo(QSize(640, 400)));
    }
}

void Browser::refreshFolder() {
    if (!browseFolder_ || currentPath_.isEmpty()) {
        folder_.clear();
        index_ = -1;
        if (chrome_ != nullptr) {
            chrome_->setFolder(folder_, index_);
        }
        return;
    }
    const QString path = currentPath_;
    const QString directory = QFileInfo(path).absolutePath();
    if (!qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        folder_ = listFolderImages(directory);
        index_ = folder_.indexOf(path);
        if (chrome_ != nullptr) {
            chrome_->setFolder(folder_, index_);
        }
        updateChrome();
        return;
    }
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread folderWorker([directory, path, self, alive] {
        const QStringList files = listFolderImages(directory);
        deliver(alive, self, [files, directory, path](Browser *browser) {
            if (!browser->browseFolder_ || QFileInfo(browser->currentPath_).absolutePath() != directory) {
                return;
            }
            browser->folder_ = files;
            browser->index_ = browser->folder_.indexOf(path);
            browser->scheduleLookahead();
            if (browser->chrome_ != nullptr) {
                browser->chrome_->setFolder(browser->folder_, browser->index_);
            }
            browser->updateChrome();
        });
    });
    folderWorker.detach();
}

void Browser::launchHost(const QStringList &tail) {
    QProcess process;
    if (!startToolsHost(process, tail)) {
        return;
    }
    process.startDetached();
}

void Browser::openTools(bool enlarge) {
    QStringList tail;
    if (enlarge) {
        tail.append(QStringLiteral("--enlarge"));
        tail.append(QStringLiteral("--frame"));
        tail.append(QString::number(frame_));
    }
    tail.append(QStringLiteral("--path"));
    tail.append(QFileInfo(currentPath_).absoluteFilePath());
    launchHost(tail);
}

void Browser::checkForUpdate() { launchHost({QStringLiteral("--check-update")}); }

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
    setCatalogLanguage(language());
    std::map<std::string, Json> own;
    own.emplace("ui_language", Json::fromString(language().toStdString()));
    std::string error;
    writeOwnKeys(suiteSettingsPath(), own, error);
    if (chrome_ != nullptr) {
        chrome_->retranslate();
    }
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
    const QRect view = pictureRect();
    if (!image_.isNull()) {
        painter.save();
        painter.setClipRect(view);
        painter.translate(view.topLeft());
        painter.translate(-placement_.originX * placement_.scale, -placement_.originY * placement_.scale);
        painter.scale(placement_.scale, placement_.scale);
        painter.drawImage(QPointF(0, 0), image_);
        painter.restore();
    } else if (!error_.isEmpty()) {
        painter.setPen(QColor(QStringLiteral("#f2f2f5")));
        painter.drawText(view.adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, error_);
    }
    if (cropping_ && cropRect_.isValid()) {
        painter.setPen(QPen(QColor(QStringLiteral("#60a5fa")), 2));
        painter.drawRect(cropRect_);
    }
}

void Browser::keyPressEvent(QKeyEvent *event) {
    if (!armed_) {
        loadHotkeys();
    }
    if (cropping_ && event->key() == Qt::Key_Escape) {
        cancelCrop();
        return;
    }
    if (cropping_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        applyCrop();
        return;
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
    case Action::Open: {
        const QString path = QFileDialog::getOpenFileName(this, text(Text::Open));
        if (!path.isEmpty()) {
            browseFolder_ = true;
            openPath(path);
        }
        break;
    }
    case Action::RotateLeft:
        rotateBy(false);
        break;
    case Action::RotateRight:
        rotateBy(true);
        break;
    case Action::FlipHorizontal:
        flipImage(true);
        break;
    case Action::FlipVertical:
        flipImage(false);
        break;
    case Action::Crop:
        beginCrop();
        break;
    case Action::SaveCopy:
        saveCopy();
        break;
    case Action::SaveAs:
        saveAs();
        break;
    case Action::Trash:
        moveToTrash();
        break;
    case Action::Enlarge:
        openTools(true);
        break;
    case Action::Settings:
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->showSettings();
        }
        break;
    case Action::CopyImage:
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->copyImage();
        }
        break;
    case Action::CopyPath:
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->copyPath();
        }
        break;
    case Action::Reveal:
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->reveal();
        }
        break;
    case Action::Gallery:
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->toggleGallery();
        }
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
    const QPointF local = event->position() - pictureRect().topLeft();
    placement_ = zoomAtCursor(placement_, std::pow(1.1, steps), local.x(), local.y());
    if (chrome_ != nullptr) {
        chrome_->noteZoom(ZoomMode::Free, placement_.scale);
    }
    ensureFullResolution();
    syncRhi();
    updateChrome();
    update();
}

void Browser::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        ensureChrome();
        if (chrome_ != nullptr) {
            chrome_->contextMenu(event->globalPosition().toPoint());
        }
        return;
    }
    if (cropping_ && event->button() == Qt::LeftButton) {
        cropDragging_ = true;
        cropAnchor_ = event->position().toPoint();
        cropRect_ = QRect(cropAnchor_, QSize(1, 1));
        update();
        return;
    }
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
    if (chrome_ != nullptr && !dragging_ && !cropDragging_) {
        chrome_->mouseAt(event->position().toPoint());
    }
    if (cropping_ && cropDragging_) {
        cropRect_ = QRect(cropAnchor_, event->position().toPoint()).normalized();
        update();
        return;
    }
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
    if (cropDragging_ && event->button() == Qt::LeftButton) {
        cropDragging_ = false;
        cropRect_ = QRect(cropAnchor_, event->position().toPoint()).normalized();
        update();
        return;
    }
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
    if (splash_ != nullptr) {
        splash_->setGeometry(rect());
        splash_->raise();
    }
    if (chrome_ != nullptr) {
        chrome_->relayout();
    }
    refit();
    updateChrome();
}

void Browser::ensureChrome() {
    if (chrome_ != nullptr) {
        return;
    }
    ChromeCallbacks callbacks;
    callbacks.openPath = [this](const QString &path) {
        browseFolder_ = true;
        openPath(path);
    };
    callbacks.step = [this](int delta) { stepFile(delta); };
    callbacks.openIndex = [this](int index) {
        if (index < 0 || index >= folder_.size()) {
            return;
        }
        index_ = index;
        openFile(folder_.at(index));
    };
    callbacks.zoom = [this](ZoomMode mode) { applyMode(mode); };
    callbacks.rotate = [this](bool right) { rotateBy(right); };
    callbacks.flip = [this](bool horizontal) { flipImage(horizontal); };
    callbacks.crop = [this] { beginCrop(); };
    callbacks.save = [this] { saveImage(); };
    callbacks.saveCopy = [this] { saveCopy(); };
    callbacks.saveAs = [this] { saveAs(); };
    callbacks.exportImage = [this] { exportImage(); };
    callbacks.download = [this] { downloadImage(); };
    callbacks.trash = [this] { moveToTrash(); };
    callbacks.slideshow = [this] { toggleSlideshow(); };
    callbacks.fullscreen = [this] { toggleFullscreen(); };
    callbacks.enlarge = [this] { openTools(true); };
    callbacks.hotkeys = [this] { editHotkeys(this); };
    callbacks.language = [this](const QString &code) {
        setLanguage(code);
        QSettings().setValue(QStringLiteral("language"), code);
        updateChrome();
    };
    callbacks.galleryVisible = [this](bool) { updateChrome(); };
    callbacks.slideshowMs = [this](int ms) {
        if (slideshow_ != nullptr) {
            slideshow_->setInterval(ms);
        }
    };
    callbacks.image = [this] { return image_; };
    callbacks.path = [this] { return currentPath_; };
    callbacks.numericCrop = [this](int x, int y, int w, int h) {
        if (image_.isNull()) {
            return;
        }
        const QRect bounds = QRect(x, y, w, h).intersected(image_.rect());
        if (bounds.width() > 1 && bounds.height() > 1) {
            image_ = image_.copy(bounds);
            showImage(image_, true);
        }
    };
    callbacks.stepFrame = [this](int delta) {
        if (animation_ != nullptr) {
            animation_->stop();
        }
        stepFrame(delta);
    };
    callbacks.togglePlay = [this] {
        if (animation_ == nullptr || frameCount_ <= 1) {
            return;
        }
        if (animation_->isActive()) {
            animation_->stop();
        } else {
            animation_->start();
        }
    };
    callbacks.setScale = [this](double scale) {
        mode_ = ZoomMode::Free;
        const QSize view = pictureRect().size();
        if (!image_.isNull()) {
            placement_ = centeredPlacement(scale, image_.width(), image_.height(), std::max(1, view.width()), std::max(1, view.height()));
        }
        if (scale > 1.05) {
            ensureFullResolution();
        }
        syncRhi();
        updateChrome();
        update();
    };
    callbacks.checkUpdate = [this] { checkForUpdate(); };
    callbacks.frameless = [this] { toggleFrameless(); };
    callbacks.windowFit = [this] { fitWindow(); };
    callbacks.tools = [this] { openTools(); };
    callbacks.resizeImage = [this](int width, int height, bool smooth) { resizeTo(width, height, smooth); };
    chrome_ = new Chrome(this, std::move(callbacks));
    chrome_->setFolder(folder_, index_);
    chrome_->prepareShots();
    refit();
    updateChrome();
}

void Browser::startSplashLoad() {
    const QPointer<Browser> self(this);
    const auto alive = alive_;
    std::thread([self, alive] {
        const QImage image = readShareImage(QStringLiteral("splash.avif"));
        deliver(alive, self, [image](Browser *browser) { browser->offerSplash(image); });
    }).detach();
}

void Browser::offerSplash(const QImage &image) {
    if (image.isNull()) {
        return;
    }
    if (qgetenv("LUCENT_SCREENSHOT") == "splash") {
        splash_ = showSplash(this, image);
        if (splash_ != nullptr) {
            splash_->setGeometry(rect());
            splash_->raise();
        }
        QTimer::singleShot(200, this, [this] {
            if (splash_ == nullptr) {
                return;
            }
            const QString dir = qEnvironmentVariable("LUCENT_SCREENSHOT_DIR", QStringLiteral("/tmp/lucent-shots"));
            QDir().mkpath(dir);
            splash_->grab().save(dir + QStringLiteral("/splash.png"));
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        });
        return;
    }
    if (imageReady_) {
        return;
    }
    splash_ = showSplash(this, image);
    if (splash_ != nullptr) {
        splash_->setGeometry(rect());
        splash_->raise();
    }
}

void Browser::maybeWelcome() {
    const QByteArray shot = qgetenv("LUCENT_SCREENSHOT");
    QSettings settings;
    const bool enabled = settings.value(QStringLiteral("welcome/enabled"), true).toBool();
    const int count = settings.value(QStringLiteral("welcome/emptyCount"), 0).toInt();
    const WelcomeDecision decision = decideWelcome(enabled, count);
    if (shot.isEmpty()) {
        settings.setValue(QStringLiteral("welcome/emptyCount"), decision.storedCount);
    }
    if (!decision.show && shot != "welcome") {
        return;
    }
    const QString path = findShareDir() + QStringLiteral("/welcome.avif");
    if (!QFileInfo::exists(path)) {
        return;
    }
    openFile(path, false);
}

void Browser::rotateBy(bool right) {
    if (image_.isNull()) {
        return;
    }
    QTransform transform;
    transform.rotate(right ? 90.0 : -90.0);
    image_ = image_.transformed(transform, Qt::SmoothTransformation);
    showImage(image_, true);
}

void Browser::flipImage(bool horizontal) {
    if (image_.isNull()) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    image_ = image_.flipped(horizontal ? Qt::Horizontal : Qt::Vertical);
#else
    image_ = image_.mirrored(horizontal, !horizontal);
#endif
    showImage(image_, true);
}

void Browser::beginCrop() {
    if (image_.isNull()) {
        return;
    }
    cropping_ = true;
    cropRect_ = {};
    if (rhi_ != nullptr) {
        rhi_->hide();
    }
    update();
}

void Browser::cancelCrop() {
    cropping_ = false;
    cropDragging_ = false;
    cropRect_ = {};
    if (rhi_ != nullptr) {
        syncRhi();
        rhi_->show();
    }
    update();
}

void Browser::applyCrop() {
    const QRect rect = cropRect_.normalized().translated(-pictureRect().topLeft());
    cropping_ = false;
    cropDragging_ = false;
    if (image_.isNull() || rect.width() < 4 || rect.height() < 4 || placement_.scale <= 0.0) {
        cancelCrop();
        return;
    }
    const int x = qRound(rect.x() / placement_.scale + placement_.originX);
    const int y = qRound(rect.y() / placement_.scale + placement_.originY);
    const int w = qRound(rect.width() / placement_.scale);
    const int h = qRound(rect.height() / placement_.scale);
    const QRect bounds = QRect(x, y, w, h).intersected(image_.rect());
    if (bounds.isValid() && bounds.width() > 1 && bounds.height() > 1) {
        image_ = image_.copy(bounds);
        showImage(image_, true);
    }
    if (rhi_ != nullptr) {
        syncRhi();
        rhi_->show();
    }
    update();
}

QString Browser::uniqueAvif(const QString &directory, const QString &base, const QString &infix) const {
    const QString ext = QLatin1Char('.') + exportSuffix();
    QString dest = directory + QLatin1Char('/') + base + infix + ext;
    int number = 2;
    while (QFileInfo::exists(dest)) {
        const QString counted = infix.isEmpty() ? QLatin1Char('-') + QString::number(number++)
                                                 : infix + QLatin1Char('-') + QString::number(number++);
        dest = directory + QLatin1Char('/') + base + counted + ext;
    }
    return dest;
}

bool Browser::writeView(const QString &path) {
    if (image_.isNull() || path.isEmpty()) {
        return false;
    }
    QImageWriter writer(path, QFileInfo(path).suffix().toLower().toUtf8());
    if (chrome_ != nullptr) {
        writer.setQuality(chrome_->exportQuality());
    }
    if (!writer.write(image_)) {
        error_ = text(Text::SaveFailed);
        updateChrome();
        update();
        return false;
    }
    error_.clear();
    updateChrome();
    return true;
}

void Browser::saveImage() {
    if (image_.isNull()) {
        return;
    }
    const QFileInfo info(currentPath_);
    QString directory = info.absolutePath();
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QString base = info.completeBaseName();
    if (base.isEmpty()) {
        base = QStringLiteral("image");
    }
    writeView(uniqueAvif(directory, base, QStringLiteral(".saved")));
}

void Browser::saveCopy() {
    if (image_.isNull()) {
        return;
    }
    const QFileInfo info(currentPath_);
    QString directory = info.absolutePath();
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QString base = info.completeBaseName();
    if (base.isEmpty()) {
        base = QStringLiteral("image");
    }
    writeView(uniqueAvif(directory, base, QStringLiteral("-copy")));
}

void Browser::exportImage() {
    if (image_.isNull()) {
        return;
    }
    const QFileInfo info(currentPath_);
    QString directory = info.absolutePath();
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QString base = info.completeBaseName();
    if (base.isEmpty()) {
        base = QStringLiteral("image");
    }
    writeView(uniqueAvif(directory, base, QStringLiteral(".export")));
}

void Browser::downloadImage() {
    if (image_.isNull()) {
        return;
    }
    QString directory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath();
    }
    QDir().mkpath(directory);
    const QFileInfo info(currentPath_);
    QString base = info.completeBaseName();
    if (base.isEmpty()) {
        base = QStringLiteral("image");
    }
    writeView(uniqueAvif(directory, base, QString()));
}

void Browser::resizeTo(int width, int height, bool smooth) {
    if (image_.isNull() || width < 1 || height < 1) {
        return;
    }
    if (width == image_.width() && height == image_.height()) {
        return;
    }
    image_ = image_.scaled(width, height, Qt::IgnoreAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
    showImage(image_, true);
}

void Browser::saveAs() {
    if (image_.isNull()) {
        return;
    }
    const QFileInfo info(currentPath_);
    const QString suggested = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QLatin1Char('.') + exportSuffix();
    QString selected;
    QString dest = QFileDialog::getSaveFileName(
        this, text(Text::SaveAs), suggested, QStringLiteral("AVIF (*.avif);;JPEG XL (*.jxl);;WebP (*.webp)"), &selected);
    if (dest.isEmpty()) {
        return;
    }
    if (QFileInfo(dest).suffix().isEmpty()) {
        if (selected.contains(QStringLiteral("jxl"))) {
            dest += QStringLiteral(".jxl");
        } else if (selected.contains(QStringLiteral("webp"), Qt::CaseInsensitive)) {
            dest += QStringLiteral(".webp");
        } else {
            dest += QStringLiteral(".avif");
        }
    }
    writeView(dest);
}

void Browser::moveToTrash() {
    if (currentPath_.isEmpty()) {
        return;
    }
    if (qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        const auto answer = QMessageBox::question(this, label(QStringLiteral("lucent.action.trash")), label(QStringLiteral("lucent.trash.confirm")));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }
    const QString share = findShareDir();
    if (!share.isEmpty() && currentPath_.startsWith(share)) {
        return;
    }
    if (!QFile::moveToTrash(currentPath_)) {
        error_ = text(Text::TrashFailed);
        updateChrome();
        update();
        return;
    }
    const QString removed = currentPath_;
    folder_.removeAll(removed);
    if (folder_.isEmpty()) {
        currentPath_.clear();
        image_ = {};
        index_ = -1;
        update();
        if (chrome_ != nullptr) {
            chrome_->setFolder(folder_, index_);
        }
        return;
    }
    if (index_ >= folder_.size()) {
        index_ = 0;
    }
    openFile(folder_.at(index_));
}

}  // namespace lucent
