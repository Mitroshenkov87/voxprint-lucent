// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "decode.h"

#include "display_hdr.h"
#include "splash.h"
#include "tone_curve.h"

#include <QColorSpace>
#include <QFile>
#include <QImageIOHandler>
#include <QImageReader>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if defined(LUCENT_HAS_LIBJXL)
#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>
#include <jxl/version.h>
#endif

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
            toneMapToSdrLinear(r, g, b, pq);
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

QImage fitView(QImage image, const QSize &box) {
    if (image.isNull() || !box.isValid()) {
        return image;
    }
    if (image.width() <= box.width() && image.height() <= box.height()) {
        return image;
    }
    return image.scaled(box, Qt::KeepAspectRatio, Qt::FastTransformation);
}

#if defined(LUCENT_HAS_LIBJXL)
int jxlThreads() {
    unsigned count = std::thread::hardware_concurrency();
    if (count < 1) {
        count = 1;
    }
    if (count > 64) {
        count = 64;
    }
    return static_cast<int>(count);
}

QImage readJxl(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 2) {
        return {};
    }
    const auto *data = reinterpret_cast<const uint8_t *>(bytes.constData());
    const size_t length = static_cast<size_t>(bytes.size());
    if (JxlSignatureCheck(data, length) == JXL_SIG_INVALID) {
        return {};
    }
    JxlDecoder *decoder = JxlDecoderCreate(nullptr);
    if (decoder == nullptr) {
        return {};
    }
    void *runner = JxlThreadParallelRunnerCreate(nullptr, static_cast<size_t>(jxlThreads()));
    if (runner != nullptr) {
        JxlDecoderSetParallelRunner(decoder, JxlThreadParallelRunner, runner);
    }
    JxlDecoderSubscribeEvents(decoder, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FULL_IMAGE);
    JxlDecoderSetInput(decoder, data, length);
    JxlDecoderCloseInput(decoder);

    JxlBasicInfo info{};
    std::vector<uint8_t> pixels;
    bool hdr = false;
    bool got = false;
    for (int step = 0; step < 16; ++step) {
        const JxlDecoderStatus status = JxlDecoderProcessInput(decoder);
        if (status == JXL_DEC_ERROR || status == JXL_DEC_NEED_MORE_INPUT) {
            break;
        }
        if (status == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(decoder, &info) != JXL_DEC_SUCCESS) {
                break;
            }
            continue;
        }
        if (status == JXL_DEC_COLOR_ENCODING) {
            JxlColorEncoding original{};
            // libjxl 0.9 dropped the unused pixel-format argument.
#if JPEGXL_NUMERIC_VERSION >= JPEGXL_COMPUTE_NUMERIC_VERSION(0, 9, 0)
            const JxlDecoderStatus colorStatus = JxlDecoderGetColorAsEncodedProfile(
                decoder, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &original);
#else
            const JxlDecoderStatus colorStatus = JxlDecoderGetColorAsEncodedProfile(
                decoder, nullptr, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &original);
#endif
            if (colorStatus == JXL_DEC_SUCCESS) {
                if (original.transfer_function == JXL_TRANSFER_FUNCTION_PQ
                    || original.transfer_function == JXL_TRANSFER_FUNCTION_HLG) {
                    hdr = true;
                    break;
                }
            }
            JxlColorEncoding preferred{};
            const JXL_BOOL gray = info.num_color_channels == 1 ? JXL_TRUE : JXL_FALSE;
            JxlColorEncodingSetToSRGB(&preferred, gray);
            JxlDecoderSetPreferredColorProfile(decoder, &preferred);
            continue;
        }
        if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            JxlPixelFormat format{};
            format.num_channels = info.num_color_channels == 1 ? 1 : 4;
            format.data_type = JXL_TYPE_UINT8;
            format.endianness = JXL_NATIVE_ENDIAN;
            format.align = 0;
            size_t size = 0;
            if (JxlDecoderImageOutBufferSize(decoder, &format, &size) != JXL_DEC_SUCCESS) {
                break;
            }
            pixels.resize(size);
            if (JxlDecoderSetImageOutBuffer(decoder, &format, pixels.data(), pixels.size())
                != JXL_DEC_SUCCESS) {
                break;
            }
            continue;
        }
        if (status == JXL_DEC_FULL_IMAGE) {
            got = true;
            break;
        }
        if (status == JXL_DEC_SUCCESS) {
            break;
        }
    }
    QImage image;
    if (!hdr && got && info.xsize > 0 && info.ysize > 0 && !pixels.empty()) {
        const bool gray = info.num_color_channels == 1;
        const int channels = gray ? 1 : 4;
        image = QImage(static_cast<int>(info.xsize),
                       static_cast<int>(info.ysize),
                       gray ? QImage::Format_Grayscale8 : QImage::Format_RGBA8888);
        const size_t stride = static_cast<size_t>(info.xsize) * static_cast<size_t>(channels);
        if (pixels.size() >= stride * info.ysize) {
            for (uint32_t y = 0; y < info.ysize; ++y) {
                std::memcpy(image.scanLine(static_cast<int>(y)), pixels.data() + y * stride, stride);
            }
        } else {
            image = {};
        }
    }
    if (runner != nullptr) {
        JxlThreadParallelRunnerDestroy(runner);
    }
    JxlDecoderDestroy(decoder);
    return image;
}
#endif

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
    return fitView(image, box);
}

}  // namespace

QImage decodeForView(const QString &path, const QSize &box) {
    if (path.endsWith(QLatin1String(".avif"), Qt::CaseInsensitive)) {
        const QImage direct = decodeAvifFile(path);
        if (!direct.isNull()) {
            return fitView(direct, box);
        }
    }
#if defined(LUCENT_HAS_LIBJXL)
    if (path.endsWith(QLatin1String(".jxl"), Qt::CaseInsensitive)) {
        const QImage direct = readJxl(path);
        if (!direct.isNull()) {
            return fitView(direct, box);
        }
    }
#endif
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
