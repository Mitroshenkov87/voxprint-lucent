// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "gallery.h"

#include "theme.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <thread>

namespace lucent {
namespace {

constexpr int kTile = 88;

QImage decodeThumb(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && !size.isEmpty()) {
        reader.setScaledSize(size.scaled(kTile, kTile, Qt::KeepAspectRatio));
    }
    return reader.read();
}

QPixmap placeholderThumb() {
    QPixmap pix(kTile - 4, kTile - 4);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor ink(0xa8, 0xa8, 0xb8, 150);
    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(22, 26, 40, 30), 3, 3);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawEllipse(QPointF(32, 35), 3.2, 3.2);
    painter.setPen(QPen(ink, 1.6));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolyline(QPolygonF{QPointF(28, 50), QPointF(38, 40), QPointF(46, 47), QPointF(56, 38)});
    return pix;
}

void showThumb(QPushButton *button, const QImage &image) {
    if (image.isNull()) {
        button->setIcon(placeholderThumb());
        return;
    }
    button->setIcon(QPixmap::fromImage(image));
}

}  // namespace

Gallery::Gallery(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("history"));
    setFixedHeight(104);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("historyScroll"));
    scroll->viewport()->setObjectName(QStringLiteral("historyViewport"));
    scroll->horizontalScrollBar()->setObjectName(QStringLiteral("historyBar"));
    polishScrollBar(scroll->horizontalScrollBar());
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    row_ = new QWidget(scroll);
    rowLayout_ = new QHBoxLayout(row_);
    rowLayout_->setContentsMargins(0, 8, 0, 8);
    rowLayout_->setSpacing(8);
    rowLayout_->addStretch(1);
    scroll->setWidget(row_);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    group_ = new QButtonGroup(this);
    group_->setExclusive(true);
}

void Gallery::rebuild() {
    for (QPushButton *button : buttons_) {
        group_->removeButton(button);
        button->deleteLater();
    }
    buttons_.clear();
    for (int index = 0; index < files_.size(); ++index) {
        auto *button = new QPushButton(row_);
        button->setProperty("thumb", true);
        button->setCheckable(true);
        button->setFixedSize(kTile, kTile);
        button->setIconSize(QSize(kTile - 4, kTile - 4));
        button->setChecked(index == current_);
        showThumb(button, thumbs_.value(files_.at(index)));
        group_->addButton(button);
        rowLayout_->insertWidget(rowLayout_->count() - 1, button);
        buttons_.append(button);
        QObject::connect(button, &QPushButton::clicked, this, [this, index] {
            current_ = index;
            if (onChosen) {
                onChosen(index);
            }
        });
    }
}

void Gallery::setFiles(const QStringList &files, int current) {
    files_ = files;
    current_ = current;
    ++generation_;
    if (!qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        for (const QString &path : files_) {
            if (!thumbs_.contains(path) || thumbs_.value(path).isNull()) {
                thumbs_.insert(path, decodeThumb(path));
            }
        }
    }
    rebuild();
    if (qgetenv("LUCENT_SCREENSHOT").isEmpty()) {
        pump();
    }
}

void Gallery::setCurrent(int current) {
    current_ = current;
    if (current >= 0 && current < buttons_.size()) {
        buttons_.at(current)->setChecked(true);
    }
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
                        const int index = self->files_.indexOf(next);
                        if (index >= 0 && index < self->buttons_.size()) {
                            showThumb(self->buttons_.at(index), image);
                        }
                    }
                    self->pump();
                },
                Qt::QueuedConnection);
        }).detach();
    }
}

}  // namespace lucent
