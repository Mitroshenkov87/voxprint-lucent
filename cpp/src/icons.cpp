// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "icons.h"

#include <QPainter>
#include <QPen>
#include <QPixmap>

namespace lucent {
namespace {

void beginIcon(QPainter &painter) {
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(QStringLiteral("#f2f2f5")));
    pen.setWidthF(1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
}

void chevron(QPainter &painter, bool left) {
    if (left) {
        painter.drawLine(QPointF(14, 5), QPointF(8, 11));
        painter.drawLine(QPointF(8, 11), QPointF(14, 17));
    } else {
        painter.drawLine(QPointF(8, 5), QPointF(14, 11));
        painter.drawLine(QPointF(14, 11), QPointF(8, 17));
    }
}

void corners(QPainter &painter, bool inward) {
    const QPointF marks[] = {{4, 8}, {4, 4}, {8, 4}, {14, 4}, {18, 4}, {18, 8},
                             {4, 14}, {4, 18}, {8, 18}, {14, 18}, {18, 18}, {18, 14}};
    for (int i = 0; i < 12; i += 3) {
        painter.drawLine(marks[i], marks[i + 1]);
        painter.drawLine(marks[i + 1], marks[i + 2]);
    }
    if (inward) {
        painter.drawLine(QPointF(8, 11), QPointF(11, 11));
        painter.drawLine(QPointF(11, 8), QPointF(11, 14));
    }
}

}  // namespace

QIcon lucentIcon(Icon id) {
    QPixmap pix(22, 22);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    beginIcon(painter);
    switch (id) {
    case Icon::Menu:
        painter.drawLine(QPointF(4, 6), QPointF(18, 6));
        painter.drawLine(QPointF(4, 11), QPointF(18, 11));
        painter.drawLine(QPointF(4, 16), QPointF(18, 16));
        break;
    case Icon::Open:
        painter.drawRect(QRectF(3, 8, 16, 10));
        painter.drawLine(QPointF(3, 8), QPointF(8, 8));
        painter.drawLine(QPointF(8, 8), QPointF(10, 5));
        painter.drawLine(QPointF(10, 5), QPointF(16, 5));
        painter.drawLine(QPointF(16, 5), QPointF(16, 8));
        break;
    case Icon::Previous:
        chevron(painter, true);
        break;
    case Icon::Next:
        chevron(painter, false);
        break;
    case Icon::Fit:
        corners(painter, true);
        break;
    case Icon::Fill:
        painter.drawRect(QRectF(5, 5, 12, 12));
        painter.drawLine(QPointF(5, 5), QPointF(2, 2));
        painter.drawLine(QPointF(17, 5), QPointF(20, 2));
        painter.drawLine(QPointF(5, 17), QPointF(2, 20));
        painter.drawLine(QPointF(17, 17), QPointF(20, 20));
        break;
    case Icon::OneToOne:
        painter.drawText(QRectF(0, 0, 22, 22), Qt::AlignCenter, QStringLiteral("1"));
        break;
    case Icon::FitWidth:
        painter.drawLine(QPointF(3, 11), QPointF(19, 11));
        painter.drawLine(QPointF(3, 11), QPointF(7, 7));
        painter.drawLine(QPointF(3, 11), QPointF(7, 15));
        painter.drawLine(QPointF(19, 11), QPointF(15, 7));
        painter.drawLine(QPointF(19, 11), QPointF(15, 15));
        break;
    case Icon::FitHeight:
        painter.drawLine(QPointF(11, 3), QPointF(11, 19));
        painter.drawLine(QPointF(11, 3), QPointF(7, 7));
        painter.drawLine(QPointF(11, 3), QPointF(15, 7));
        painter.drawLine(QPointF(11, 19), QPointF(7, 15));
        painter.drawLine(QPointF(11, 19), QPointF(15, 15));
        break;
    case Icon::RotateLeft:
        painter.drawArc(QRectF(5, 5, 12, 12), 40 * 16, 260 * 16);
        painter.drawLine(QPointF(5, 11), QPointF(5, 6));
        painter.drawLine(QPointF(5, 11), QPointF(9, 11));
        break;
    case Icon::RotateRight:
        painter.drawArc(QRectF(5, 5, 12, 12), 40 * 16, 260 * 16);
        painter.drawLine(QPointF(17, 11), QPointF(17, 6));
        painter.drawLine(QPointF(17, 11), QPointF(13, 11));
        break;
    case Icon::FlipHorizontal:
        painter.drawLine(QPointF(11, 4), QPointF(11, 18));
        painter.drawLine(QPointF(4, 7), QPointF(8, 11));
        painter.drawLine(QPointF(4, 15), QPointF(8, 11));
        painter.drawLine(QPointF(18, 7), QPointF(14, 11));
        painter.drawLine(QPointF(18, 15), QPointF(14, 11));
        break;
    case Icon::FlipVertical:
        painter.drawLine(QPointF(4, 11), QPointF(18, 11));
        painter.drawLine(QPointF(7, 4), QPointF(11, 8));
        painter.drawLine(QPointF(15, 4), QPointF(11, 8));
        painter.drawLine(QPointF(7, 18), QPointF(11, 14));
        painter.drawLine(QPointF(15, 18), QPointF(11, 14));
        break;
    case Icon::Crop:
        painter.drawLine(QPointF(7, 4), QPointF(7, 8));
        painter.drawLine(QPointF(4, 7), QPointF(8, 7));
        painter.drawLine(QPointF(15, 4), QPointF(15, 8));
        painter.drawLine(QPointF(14, 7), QPointF(18, 7));
        painter.drawLine(QPointF(7, 14), QPointF(7, 18));
        painter.drawLine(QPointF(4, 15), QPointF(8, 15));
        painter.drawLine(QPointF(15, 14), QPointF(15, 18));
        painter.drawLine(QPointF(14, 15), QPointF(18, 15));
        break;
    case Icon::Save:
        painter.drawRect(QRectF(4, 3, 14, 16));
        painter.drawLine(QPointF(7, 3), QPointF(7, 8));
        painter.drawLine(QPointF(7, 8), QPointF(15, 8));
        painter.drawRect(QRectF(7, 12, 8, 5));
        break;
    case Icon::SaveAs:
        painter.drawRect(QRectF(3, 8, 12, 10));
        painter.drawLine(QPointF(13, 5), QPointF(18, 5));
        painter.drawLine(QPointF(15.5, 2.5), QPointF(15.5, 7.5));
        break;
    case Icon::Trash:
        painter.drawLine(QPointF(5, 7), QPointF(17, 7));
        painter.drawRect(QRectF(7, 7, 8, 11));
        painter.drawLine(QPointF(9, 4), QPointF(13, 4));
        painter.drawLine(QPointF(10, 10), QPointF(10, 15));
        painter.drawLine(QPointF(12, 10), QPointF(12, 15));
        break;
    case Icon::Slideshow:
        painter.drawLine(QPointF(8, 5), QPointF(16, 11));
        painter.drawLine(QPointF(16, 11), QPointF(8, 17));
        painter.drawLine(QPointF(8, 17), QPointF(8, 5));
        break;
    case Icon::Fullscreen:
        painter.drawLine(QPointF(4, 8), QPointF(4, 4));
        painter.drawLine(QPointF(4, 4), QPointF(8, 4));
        painter.drawLine(QPointF(14, 4), QPointF(18, 4));
        painter.drawLine(QPointF(18, 4), QPointF(18, 8));
        painter.drawLine(QPointF(4, 14), QPointF(4, 18));
        painter.drawLine(QPointF(4, 18), QPointF(8, 18));
        painter.drawLine(QPointF(14, 18), QPointF(18, 18));
        painter.drawLine(QPointF(18, 18), QPointF(18, 14));
        break;
    case Icon::Gallery:
        painter.drawRect(QRectF(2, 7, 5, 8));
        painter.drawRect(QRectF(8.5, 7, 5, 8));
        painter.drawRect(QRectF(15, 7, 5, 8));
        break;
    case Icon::Enlarge:
        painter.drawEllipse(QRectF(3, 3, 10, 10));
        painter.drawLine(QPointF(11.5, 11.5), QPointF(18, 18));
        painter.drawLine(QPointF(6, 8), QPointF(10, 8));
        painter.drawLine(QPointF(8, 6), QPointF(8, 10));
        break;
    case Icon::Settings:
        painter.drawEllipse(QRectF(8, 8, 6, 6));
        painter.drawLine(QPointF(11, 2), QPointF(11, 5));
        painter.drawLine(QPointF(11, 17), QPointF(11, 20));
        painter.drawLine(QPointF(2, 11), QPointF(5, 11));
        painter.drawLine(QPointF(17, 11), QPointF(20, 11));
        painter.drawLine(QPointF(4.5, 4.5), QPointF(6.5, 6.5));
        painter.drawLine(QPointF(15.5, 15.5), QPointF(17.5, 17.5));
        painter.drawLine(QPointF(15.5, 6.5), QPointF(17.5, 4.5));
        painter.drawLine(QPointF(4.5, 17.5), QPointF(6.5, 15.5));
        break;
    case Icon::Copy:
        painter.drawRect(QRectF(7, 6, 10, 12));
        painter.drawRect(QRectF(4, 3, 10, 12));
        break;
    case Icon::Path:
        painter.drawLine(QPointF(4, 6), QPointF(18, 6));
        painter.drawLine(QPointF(4, 11), QPointF(14, 11));
        painter.drawLine(QPointF(4, 16), QPointF(16, 16));
        break;
    case Icon::Reveal:
        painter.drawRect(QRectF(3, 8, 16, 10));
        painter.drawLine(QPointF(3, 8), QPointF(8, 8));
        painter.drawLine(QPointF(8, 8), QPointF(10, 5));
        painter.drawLine(QPointF(10, 5), QPointF(14, 5));
        painter.drawLine(QPointF(12, 13), QPointF(16, 9));
        painter.drawLine(QPointF(14, 9), QPointF(16, 9));
        painter.drawLine(QPointF(16, 9), QPointF(16, 11));
        break;
    }
    painter.end();
    return QIcon(pix);
}

}  // namespace lucent
