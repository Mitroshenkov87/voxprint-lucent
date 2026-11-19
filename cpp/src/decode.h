// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace lucent {

/// Decode `path` for a view of about `box`.
///
/// JPEG uses DCT scaling to the view. AVIF and JPEG XL use a linked libavif or libjxl
/// decode on every core when this build has those libraries, then a fast fit to
/// `box`. The plugins remain the fallback, including for PQ and HLG stills.
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
