// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "splash.h"

#include "lucent_build.h"

#include "theme.h"

#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QWidget>

#include <algorithm>
#include <cstring>

#if defined(LUCENT_HAS_LIBAVIF)
#include <avif/avif.h>
#endif

namespace lucent {
namespace {

#if defined(LUCENT_HAS_LIBAVIF)
QImage readAvif(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    avifImage *image = avifImageCreateEmpty();
    avifDecoder *decoder = avifDecoderCreate();
    if (image == nullptr || decoder == nullptr) {
        avifImageDestroy(image);
        avifDecoderDestroy(decoder);
        return {};
    }
    const avifResult read = avifDecoderReadMemory(
        decoder, image, reinterpret_cast<const uint8_t *>(bytes.constData()), static_cast<size_t>(bytes.size()));
    if (read != AVIF_RESULT_OK) {
        avifDecoderDestroy(decoder);
        avifImageDestroy(image);
        return {};
    }
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK || avifImageYUVToRGB(image, &rgb) != AVIF_RESULT_OK) {
        avifRGBImageFreePixels(&rgb);
        avifDecoderDestroy(decoder);
        avifImageDestroy(image);
        return {};
    }
    QImage converted(static_cast<int>(rgb.width), static_cast<int>(rgb.height), QImage::Format_RGBA8888);
    for (uint32_t y = 0; y < rgb.height; ++y) {
        memcpy(converted.scanLine(static_cast<int>(y)), rgb.pixels + y * rgb.rowBytes, converted.bytesPerLine());
    }
    avifRGBImageFreePixels(&rgb);
    avifDecoderDestroy(decoder);
    avifImageDestroy(image);
    return converted;
}
#endif

QRect cover(const QSize &image, const QSize &view) {
    if (image.isEmpty() || view.isEmpty()) {
        return {};
    }
    const double scale = std::max(view.width() / static_cast<double>(image.width()),
                                   view.height() / static_cast<double>(image.height()));
    const int w = static_cast<int>(image.width() * scale);
    const int h = static_cast<int>(image.height() * scale);
    return QRect((view.width() - w) / 2, (view.height() - h) / 2, w, h);
}

}  // namespace

QImage decodeAvifFile(const QString &path) {
#if defined(LUCENT_HAS_LIBAVIF)
    return readAvif(path);
#else
    Q_UNUSED(path);
    return {};
#endif
}

SplashFrame::SplashFrame(const QImage &image, QWidget *parent) : QWidget(parent), image_(image) {
    setObjectName(QStringLiteral("lucent-splash"));
}

void SplashFrame::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x17, 0x17, 0x1c));
    if (!image_.isNull()) {
        painter.drawImage(cover(image_.size(), size()), image_);
    }
    const QString title = QStringLiteral("Voxprint AI Lucent");
    const QString version = QString::fromUtf8(LUCENT_BUILD_LABEL);
    QFont font = painter.font();
    font.setPixelSize(22);
    painter.setFont(font);
    painter.setPen(QColor(0x17, 0x17, 0x1c, 180));
    painter.drawText(QRect(29, height() - 91, width() - 56, 28), Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setPen(QColor(QStringLiteral("#f2f2f5")));
    painter.drawText(QRect(28, height() - 92, width() - 56, 28), Qt::AlignLeft | Qt::AlignVCenter, title);
    font.setPixelSize(14);
    painter.setFont(font);
    painter.setPen(QColor(QStringLiteral("#c4c4d0")));
    painter.drawText(QRect(28, height() - 60, width() - 56, 22), Qt::AlignLeft | Qt::AlignVCenter, version);
}

QImage readShareImage(const QString &fileName) {
    const QString share = findShareDir();
    if (share.isEmpty()) {
        return {};
    }
    const QString path = share + QLatin1Char('/') + fileName;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (!image.isNull()) {
        return image;
    }
#if defined(LUCENT_HAS_LIBAVIF)
    return readAvif(path);
#else
    return {};
#endif
}

SplashFrame *showSplash(QWidget *parent, const QImage &image) {
    if (parent == nullptr || image.isNull()) {
        return nullptr;
    }
    auto *splash = new SplashFrame(image, parent);
    splash->setGeometry(parent->rect());
    splash->show();
    splash->raise();
    return splash;
}

}  // namespace lucent
