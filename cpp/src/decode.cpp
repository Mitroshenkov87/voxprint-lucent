// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "decode.h"

#include "splash.h"

#include <QColorSpace>
#include <QFile>
#include <QImageIOHandler>
#include <QImageReader>

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#if defined(LUCENT_HAS_LIBJXL)
#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/thread_parallel_runner.h>
#endif

namespace lucent {
namespace {

// Embedded profiles become sRGB here. Float HDR is then 8-bit. No monitor profile.
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
            if (JxlDecoderGetColorAsEncodedProfile(
                    decoder, nullptr, JXL_COLOR_PROFILE_TARGET_ORIGINAL, &original)
                == JXL_DEC_SUCCESS) {
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
