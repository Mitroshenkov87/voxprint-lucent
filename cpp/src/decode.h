// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace lucent {

// JPEG uses DCT scaling to the view. JPEG XL requests the same when the plugin
// supports ScaledSize (progressive / low-resolution first pass). Other formats
// decode full size, then scale. AVIF and JPEG XL plugins thread internally.
QImage decodeForView(const QString &path, const QSize &box);
QImage decodePreview(const QString &path, const QSize &box);
int frameCount(const QString &path);
int frameDelayMs(const QString &path, int frame);
QImage decodeFrame(const QString &path, int frame, const QSize &box);

}  // namespace lucent
