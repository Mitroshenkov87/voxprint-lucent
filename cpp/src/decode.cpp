// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "decode.h"

#include <QImageReader>

#include <cstdio>

namespace lucent {

QImage decodeForView(const QString &path, const QSize &box) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize source = reader.size();
    const QByteArray format = reader.format();
    const bool jpeg = format == "jpeg" || format == "jpg";
    if (jpeg && source.isValid() && box.isValid()) {
        reader.setScaledSize(source.scaled(box, Qt::KeepAspectRatio));
    }
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

}  // namespace lucent
