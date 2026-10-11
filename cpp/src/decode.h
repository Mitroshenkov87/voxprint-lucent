// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace lucent {

/// Decode `path` for a view of about `box`.
///
/// JPEG uses DCT scaling to the view. JPEG XL requests the same when the plugin
/// supports ScaledSize (progressive / low-resolution first pass). Other formats
/// decode full size, then scale. AVIF and JPEG XL plugins thread internally.
QImage decodeForView(const QString &path, const QSize &box);

/// Smaller decode used for thumbnails and the next-file lookahead.
///
/// PQ and HLG are tone-mapped to sRGB unless the desktop is already HDR.
QImage decodePreview(const QString &path, const QSize &box);

/// Number of frames in `path`. A still image returns 1.
int frameCount(const QString &path);

/// Delay before the next frame, in milliseconds. A still image returns 0.
int frameDelayMs(const QString &path, int frame);

/// Decode frame `frame` of `path`, scaled toward `box`.
QImage decodeFrame(const QString &path, int frame, const QSize &box);

}  // namespace lucent
