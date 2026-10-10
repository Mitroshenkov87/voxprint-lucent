// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

namespace lucent {

// Reads share/<fileName>. QImageReader first, then libavif when this build has it.
// The benchmark path never calls this.
QImage readShareImage(const QString &fileName);
QImage decodeAvifFile(const QString &path);

class SplashFrame : public QWidget {
public:
    SplashFrame(const QImage &image, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage image_;
};

// Covers `parent` until the caller closes it. Does not decode; pass an image
// that was already read off the GUI thread.
SplashFrame *showSplash(QWidget *parent, const QImage &image);

}  // namespace lucent
