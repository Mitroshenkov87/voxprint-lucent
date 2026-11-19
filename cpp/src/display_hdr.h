// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace lucent {

/// True when Windows reports an HDR desktop on an attached output.
///
/// Linux builds return false. The caller then tone-maps PQ and HLG for an SDR screen.
bool displayPrefersHdr();

}  // namespace lucent
