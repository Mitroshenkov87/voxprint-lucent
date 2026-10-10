// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

namespace lucent {

/// Read share/`fileName`. QImageReader first, then libavif when this build has it.
///
/// The benchmark path never calls this.
QImage readShareImage(const QString &fileName);

/// Decode an AVIF file with libavif. Returns a null image when the build has no libavif.
QImage decodeAvifFile(const QString &path);

/// Painted cover shown over the viewer until the first image is ready.
class SplashFrame : public QWidget {
public:
    /// Cover `parent` with `image`. The image is already decoded.
    SplashFrame(const QImage &image, QWidget *parent);

protected:
    /// Paint the cover image, letterboxed.
    void paintEvent(QPaintEvent *event) override;

private:
    QImage image_;
};

/// Cover `parent` until the caller closes the returned widget.
///
/// Does not decode; pass an image that was already read off the GUI thread.
SplashFrame *showSplash(QWidget *parent, const QImage &image);

}  // namespace lucent
