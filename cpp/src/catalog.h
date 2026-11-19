// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>
#include <QStringList>

namespace lucent {

/// True when `path` has a suffix the viewer will try to open.
bool isViewableImage(const QString &path);

/// Image files in `directory`, in natural order. The directory itself is not listed.
QStringList listFolderImages(const QString &directory);

}  // namespace lucent
