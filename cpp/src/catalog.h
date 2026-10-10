// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>
#include <QStringList>

namespace lucent {

bool isViewableImage(const QString &path);
QStringList listFolderImages(const QString &directory);

}  // namespace lucent
