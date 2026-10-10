// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "gallery.h"

#include <QImageReader>

#include <algorithm>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>

#include <thread>

namespace lucent {
namespace {

constexpr int kCell = 84;
constexpr int kThumbW = 72;
constexpr int kThumbH = 54;

QImage decodeThumb(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && !size.isEmpty()) {
        reader.setScaledSize(size.scaled(kThumbW, kThumbH, Qt::KeepAspectRatio));
    }
    return reader.read();
}

}  // namespace

Gallery::Gallery(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("lucent-gallery"));
    setFixedHeight(76);
    setMouseTracking(true);
}

void Gallery::setFiles(const QStringList &files, int current) {
    files_ = files;
    current_ = current;
    ++generation_;
    update();
    pump();
}

void Gallery::setCurrent(int current) {
    current_ = current;
    update();
}

void Gallery::pump() {
    const int generation = generation_;
    while (workers_ < 2) {
        QString next;
        for (const QString &path : files_) {
            if (!thumbs_.contains(path) && !pending_.contains(path)) {
                next = path;
                break;
            }
        }
        if (next.isEmpty()) {
            return;
        }
        pending_.insert(next, generation);
        ++workers_;
        const QPointer<Gallery> self(this);
        std::thread([self, next, generation] {
            const QImage image = decodeThumb(next);
            if (self.isNull()) {
                return;
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self, next, image, generation] {
                    if (self.isNull()) {
                        return;
                    }
                    self->pending_.remove(next);
                    --self->workers_;
                    if (generation == self->generation_) {
                        self->thumbs_.insert(next, image);
                        self->update();
                    }
                    self->pump();
                },
                Qt::QueuedConnection);
        }).detach();
    }
}

int Gallery::indexAt(int x) const {
    if (files_.isEmpty()) {
        return -1;
    }
    const int visible = std::max(1, width() / kCell);
    int first = 0;
    if (current_ >= 0) {
        first = current_ - visible / 2;
    }
    if (first < 0) {
        first = 0;
    }
    if (first + visible > files_.size()) {
        first = std::max(0, static_cast<int>(files_.size() - visible));
    }
    const int index = first + x / kCell;
    if (index < first || index >= files_.size() || index >= first + visible) {
        return -1;
    }
    return index;
}

void Gallery::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(16, 16, 22, 210));
    painter.setPen(QColor(QStringLiteral("#4b4b5c")));
    painter.drawLine(0, 0, width(), 0);
    if (files_.isEmpty()) {
        return;
    }
    const int visible = std::max(1, width() / kCell);
    int first = 0;
    if (current_ >= 0) {
        first = current_ - visible / 2;
    }
    if (first < 0) {
        first = 0;
    }
    if (first + visible > files_.size()) {
        first = std::max(0, static_cast<int>(files_.size() - visible));
    }
    for (int i = 0; i < visible && first + i < files_.size(); ++i) {
        const int index = first + i;
        const QRect cell(i * kCell + 6, 8, kThumbW, kThumbH);
        const QImage thumb = thumbs_.value(files_.at(index));
        painter.fillRect(cell, QColor(QStringLiteral("#202029")));
        if (!thumb.isNull()) {
            const QPoint topLeft(cell.x() + (cell.width() - thumb.width()) / 2,
                                 cell.y() + (cell.height() - thumb.height()) / 2);
            painter.drawImage(topLeft, thumb);
        }
        if (index == current_) {
            painter.setPen(QPen(QColor(QStringLiteral("#60a5fa")), 2));
            painter.drawRect(cell.adjusted(-1, -1, 1, 1));
        }
    }
}

void Gallery::mousePressEvent(QMouseEvent *event) {
    const int index = indexAt(static_cast<int>(event->position().x()));
    if (index >= 0 && onChosen) {
        onChosen(index);
    }
}

}  // namespace lucent
