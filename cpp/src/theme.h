// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QColor>
#include <QString>

namespace lucent {

/// Directory that holds tokens.json and the stylesheet template.
QString findShareDir();

/// Stylesheet from the generated lucent.qss, or from tokens.json and lucent.qss.in.
QString loadStyleSheet();

/// Window background colour from the ROOT_PLAIN token.
QColor rootPlain();

}  // namespace lucent
