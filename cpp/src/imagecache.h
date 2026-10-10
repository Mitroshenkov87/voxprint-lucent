// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>

namespace lucent {

class PreviewCache {
public:
    void put(const QString &key, const QImage &image);
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
