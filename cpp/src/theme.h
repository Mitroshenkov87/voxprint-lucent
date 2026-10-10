// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QColor>
#include <QString>

namespace lucent {

/// Directory that holds palette.json and the stylesheet template.
QString findShareDir();

/// Stylesheet built from share/palette.json and lucent.qss.in.
QString loadStyleSheet();

/// Window background colour from the ROOT_PLAIN token.
QColor rootPlain();

}  // namespace lucent
