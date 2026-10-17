// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>

namespace lucent {

/// Load shell and Lucent catalogs from `shareDir`.
void loadCatalogs(const QString &shareDir);

/// Active catalog language. `en`, `ru`, and `de` are the app overlays.
QString catalogLanguage();

/// Switch catalogs. Unknown codes fall back to English.
void setCatalogLanguage(const QString &code);

/// Resolve `key`. App language, shell language, app English, shell English, then the key.
QString label(const QString &key);

/// Replace `{name}` in `pattern`.
QString labelWithName(const QString &pattern, const QString &name);

}  // namespace lucent
