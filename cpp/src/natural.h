// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string_view>

namespace lucent {

/// Case-insensitive order with integer runs compared by value.
///
/// Windows folder order uses StrCmpLogicalW instead; see catalog.cpp.
int naturalCompare(std::string_view left, std::string_view right);

}  // namespace lucent
