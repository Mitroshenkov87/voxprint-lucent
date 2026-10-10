// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "imagecache.h"

namespace lucent {

void PreviewCache::dropOldest() {
    while (bytes_ > kCapBytes && !order_.isEmpty()) {
        const QString key = order_.takeFirst();
        const auto found = items_.find(key);
        if (found == items_.end()) {
            continue;
        }
        bytes_ -= found->bytes;
        items_.erase(found);
    }
}

void PreviewCache::put(const QString &key, const QImage &image) {
    if (image.isNull()) {
        return;
    }
    const qsizetype bytes = image.sizeInBytes();
    const auto existing = items_.find(key);
    if (existing != items_.end()) {
        bytes_ -= existing->bytes;
        order_.removeAll(key);
    }
    items_.insert(key, Item{image, bytes});
    order_.append(key);
    bytes_ += bytes;
    dropOldest();
}

QImage PreviewCache::get(const QString &key) const {
    const auto found = items_.constFind(key);
    if (found == items_.constEnd()) {
        return {};
    }
    return found->image;
}

}  // namespace lucent
