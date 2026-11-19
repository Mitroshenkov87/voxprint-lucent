// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "viewer.h"

#include "decode.h"
#include "theme.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPainter>
#include <QTimer>

#include <cstdio>
#include <thread>
#include <unordered_map>

namespace lucent {
namespace {

long long elapsedMs(std::chrono::steady_clock::time_point started) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - started)
        .count();
}

class ImageCache {
public:
    void put(const QString &path, const QImage &image) {
        items_.insert_or_assign(path, image);
        if (items_.size() > 5) {
            items_.erase(items_.begin());
        }
    }

    QImage get(const QString &path) const {
        const auto found = items_.find(path);
        if (found == items_.end()) {
            return {};
        }
        return found->second;
    }

private:
    std::unordered_map<QString, QImage> items_;
};

}  // namespace

Viewer::Viewer(LaunchOptions options, std::chrono::steady_clock::time_point started, QWidget *parent)
    : QWidget(parent), options_(std::move(options)), started_(started) {
    setObjectName(QStringLiteral("lucent-window"));
    setWindowTitle(QStringLiteral("VOXPrint AI Lucent"));
    setStyleSheet(loadStyleSheet());
    resize(1600, 1000);
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Detached on purpose: the benchmark path quits from paintEvent, and joining
    // there would deadlock the GUI thread. M1 should own these workers.
    openPath(options_.image);
}

void Viewer::openPath(const QString &path) {
    auto *self = this;
    const QSize box = size();
    std::thread worker([self, path, box] {
        QImage decoded = decodeForView(path, box);
        QMetaObject::invokeMethod(
            self, [self, decoded = std::move(decoded)] { self->present(decoded, false); },
            Qt::QueuedConnection);
    });
    worker.detach();
}

Viewer::~Viewer() = default;

void Viewer::present(const QImage &image, bool navigateFrame) {
    if (image.isNull()) {
        std::fprintf(stderr, "decode failed\n");
        std::fflush(stderr);
        QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(4); });
        return;
    }
    image_ = image;
    if (navigateFrame) {
        navigateArmed_ = true;
        navigateStart_ = std::chrono::steady_clock::now();
    }
    update();
}

void Viewer::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), rootPlain());
    if (!image_.isNull()) {
        const int x = (width() - image_.width()) / 2;
        const int y = (height() - image_.height()) / 2;
        painter.drawImage(x, y, image_);
    }
    painter.end();
    if (image_.isNull() || !options_.benchmark) {
        return;
    }
    if (navigateArmed_) {
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - navigateStart_)
                              .count();
        std::printf("NAVIGATE_MS %.3f\n", ms);
        std::fflush(stdout);
        navigateArmed_ = false;
        QTimer::singleShot(0, qApp, &QApplication::quit);
        return;
    }
    if (sawImage_) {
        return;
    }
    sawImage_ = true;
    std::printf("FIRST_PIXEL\n");
    std::printf("LUCENT_PHASE first_image_presented_ms %lld\n", static_cast<long long>(elapsedMs(started_)));
    std::fflush(stdout);
    if (options_.navigateNext.isEmpty()) {
        QTimer::singleShot(0, qApp, &QApplication::quit);
        return;
    }
    const QString next = options_.navigateNext;
    const QSize box = size();
    auto *self = this;
    std::thread worker([self, next, box] {
        static ImageCache cache;
        QImage cached = cache.get(next);
        if (cached.isNull()) {
            cached = decodeForView(next, box);
            cache.put(next, cached);
        }
        QMetaObject::invokeMethod(
            self, [self, cached = std::move(cached)] { self->present(cached, true); },
            Qt::QueuedConnection);
    });
    worker.detach();
}

void Viewer::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Q) {
        close();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace lucent
