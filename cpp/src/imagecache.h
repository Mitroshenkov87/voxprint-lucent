// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>

namespace lucent {

/// Memory of recent preview decodes, capped at about 256 MiB.
class PreviewCache {
public:
    /// Store `image` under `key`, dropping the oldest entry when over the cap.
    void put(const QString &key, const QImage &image);
    /// Return the image stored for `key`, or a null image.
    QImage get(const QString &key) const;

private:
    static constexpr qsizetype kCapBytes = 256LL * 1024LL * 1024LL;

    struct Item {
        QImage image;
        qsizetype bytes = 0;
    };

    void dropOldest();

    QHash<QString, Item> items_;
    QStringList order_;
    qsizetype bytes_ = 0;
};

}  // namespace lucent
