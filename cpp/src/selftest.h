// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>

namespace lucent {

/// Print one JSON object and return 0 when every required check passed.
///
/// No window is created. JPEG is required. WebP, AVIF, and JPEG XL are reported
/// and do not fail the command when this build does not include them.
int runSelfTest();

/// Sentences for a missing required runtime file. Empty when startup can continue.
QString runtimeWarning();

}  // namespace lucent
