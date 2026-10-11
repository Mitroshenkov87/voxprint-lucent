// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "decode.h"

#include "display_hdr.h"
#include "splash.h"
#include "tone_curve.h"

#include <QColorSpace>
#include <QImageIOHandler>
#include <QImageReader>

#include <cmath>
#include <cstdio>

namespace lucent {
namespace {

QImage legacySdr(QImage image) {
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

void encodedToNits(float &r, float &g, float &b, bool pq) {
    if (pq) {
        r = pqToNits(r);
        g = pqToNits(g);
        b = pqToNits(b);
        return;
    }
    const float sceneY = std::max(bt2020Luma(r, g, b), 1.0e-6F);
    r = hlgToNits(r, sceneY);
    g = hlgToNits(g, sceneY);
    b = hlgToNits(b, sceneY);
}

QImage hdrToSdr8(const QImage &image, bool pq) {
    const QImage src = image.convertToFormat(QImage::Format_RGBX32FPx4);
    QImage out(src.size(), QImage::Format_RGB32);
    for (int y = 0; y < src.height(); ++y) {
        const auto *in = reinterpret_cast<const float *>(src.constScanLine(y));
        auto *pixels = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            float r = in[x * 4];
            float g = in[x * 4 + 1];
            float b = in[x * 4 + 2];
            encodedToNits(r, g, b, pq);
            const float luma = bt2020Luma(r, g, b);
            const float scale = luma > 1.0e-3F ? sdrShoulder(luma) / luma : 0.0F;
            r *= scale;
            g *= scale;
            b *= scale;
            bt2020ToSrgb(r, g, b);
            mapGamut(r, g, b, 1.0F);
            pixels[x] = qRgb(static_cast<int>(std::lround(srgbEncode(r) * 255.0F)),
                             static_cast<int>(std::lround(srgbEncode(g) * 255.0F)),
                             static_cast<int>(std::lround(srgbEncode(b) * 255.0F)));
        }
    }
    out.setColorSpace(QColorSpace(QColorSpace::SRgb));
    return out;
}

QImage hdrToScRgb(const QImage &image, bool pq) {
    const QImage src = image.convertToFormat(QImage::Format_RGBX32FPx4);
    QImage out(src.size(), QImage::Format_RGBX32FPx4);
    for (int y = 0; y < src.height(); ++y) {
        const auto *in = reinterpret_cast<const float *>(src.constScanLine(y));
        auto *pixels = reinterpret_cast<float *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            float r = in[x * 4];
            float g = in[x * 4 + 1];
            float b = in[x * 4 + 2];
            encodedToNits(r, g, b, pq);
            r /= 203.0F;
            g /= 203.0F;
            b /= 203.0F;
            bt2020ToSrgb(r, g, b);
            mapGamut(r, g, b, 10000.0F / 203.0F);
            pixels[x * 4] = r;
            pixels[x * 4 + 1] = g;
            pixels[x * 4 + 2] = b;
            pixels[x * 4 + 3] = 1.0F;
        }
    }
    return out.convertToFormat(QImage::Format_RGBA16FPx4);
}

QImage sdrToScRgb(const QImage &image) {
    const QImage src = legacySdr(image).convertToFormat(QImage::Format_RGB32);
    QImage out(src.size(), QImage::Format_RGBX32FPx4);
    for (int y = 0; y < src.height(); ++y) {
        const auto *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        auto *pixels = reinterpret_cast<float *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            pixels[x * 4] = srgbDecode(static_cast<float>(qRed(in[x])) / 255.0F);
            pixels[x * 4 + 1] = srgbDecode(static_cast<float>(qGreen(in[x])) / 255.0F);
            pixels[x * 4 + 2] = srgbDecode(static_cast<float>(qBlue(in[x])) / 255.0F);
            pixels[x * 4 + 3] = 1.0F;
        }
    }
    return out.convertToFormat(QImage::Format_RGBA16FPx4);
}

// PQ and HLG are tone-mapped for an SDR desktop. An HDR desktop gets linear
// scRGB instead, with 203 nits at 1.0 so highlights can go above 1.
QImage toSdr(QImage image) {
    if (image.isNull()) {
        return image;
    }
    switch (image.colorSpace().transferFunction()) {
    case QColorSpace::TransferFunction::St2084:
        return displayPrefersHdr() ? hdrToScRgb(image, true) : hdrToSdr8(image, true);
    case QColorSpace::TransferFunction::Hlg:
        return displayPrefersHdr() ? hdrToScRgb(image, false) : hdrToSdr8(image, false);
    case QColorSpace::TransferFunction::Custom:
    case QColorSpace::TransferFunction::Linear:
    case QColorSpace::TransferFunction::Gamma:
    case QColorSpace::TransferFunction::SRgb:
    case QColorSpace::TransferFunction::ProPhotoRgb:
    case QColorSpace::TransferFunction::Bt2020:
        return displayPrefersHdr() ? sdrToScRgb(image) : legacySdr(image);
    }
    return legacySdr(image);
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

QImage finishRead(QImageReader &reader, const QString &path, const QSize &box) {
    QImage image = reader.read();
    if (image.isNull()) {
        image = decodeAvifFile(path);
    }
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
    return finishRead(reader, path, box);
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
    return toSdr(finishRead(reader, path, box));
}

}  // namespace lucent
