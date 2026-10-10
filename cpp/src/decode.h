// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace lucent {

// JPEG uses DCT scaling to the view. Other formats decode full size, then scale.
QImage decodeForView(const QString &path, const QSize &box);

}  // namespace lucent
