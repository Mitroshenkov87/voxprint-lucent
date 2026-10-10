// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "decode.h"

#include <QColorSpace>
#include <QImageIOHandler>
#include <QImageReader>

#include <cstdio>

namespace lucent {
namespace {

QImage toSdr(QImage image) {
    if (image.isNull()) {
        return image;
    }
    const QColorSpace srgb(QColorSpace::SRgb);
    if (image.colorSpace().isValid() && image.colorSpace() != srgb) {
        image.convertToColorSpace(srgb);
    }
    const QImage::Format format = image.format();
    if (format == QImage::Format_RGBX16FPx4 || format == QImage::Format_RGBA16FPx4
        || format == QImage::Format_RGBX32FPx4 || format == QImage::Format_RGBA32FPx4) {
        image = image.convertToFormat(QImage::Format_RGB32);
    }
    return image;
}

void applyPreviewScale(QImageReader &reader, const QSize &box) {
    if (!box.isValid()) {
        return;
    }
    const QByteArray format = reader.format();
    const QSize source = reader.size();
    const bool jpeg = format == "jpeg" || format == "jpg";
    if (jpeg && source.isValid()) {
        reader.setScaledSize(source.scaled(box, Qt::KeepAspectRatio));
        return;
    }
    if (format == "jxl" && reader.supportsOption(QImageIOHandler::ScaledSize)) {
        const QSize target = source.isValid() ? source.scaled(box, Qt::KeepAspectRatio) : box;
        if (target.isValid()) {
            reader.setScaledSize(target);
        }
    }
}

QImage finishRead(QImageReader &reader, const QSize &box) {
    QImage image = reader.read();
    if (image.isNull()) {
        std::fprintf(stderr,
                     "decode failed: format=%s error=%s\n",
                     reader.format().constData(),
                     qPrintable(reader.errorString()));
        std::fflush(stderr);
        return image;
    }
    if (box.isValid() && (image.width() > box.width() || image.height() > box.height())) {
        image = image.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

}  // namespace

QImage decodeForView(const QString &path, const QSize &box) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    applyPreviewScale(reader, box);
    return finishRead(reader, box);
}

QImage decodePreview(const QString &path, const QSize &box) {
    return toSdr(decodeForView(path, box));
}

int frameCount(const QString &path) {
    QImageReader reader(path);
    const int count = reader.imageCount();
    return count > 0 ? count : 1;
}

int frameDelayMs(const QString &path, int frame) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (frame > 0) {
        reader.jumpToImage(frame);
    }
    const int delay = reader.nextImageDelay();
    return delay > 0 ? delay : 100;
}

QImage decodeFrame(const QString &path, int frame, const QSize &box) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (frame > 0 && !reader.jumpToImage(frame)) {
        std::fprintf(stderr, "decode failed: frame %d\n", frame);
        std::fflush(stderr);
        return {};
    }
    applyPreviewScale(reader, box);
    return toSdr(finishRead(reader, box));
}

}  // namespace lucent
