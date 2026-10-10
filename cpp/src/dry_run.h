// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>
#include <QStringList>

namespace lucent {

/// True when `argv` contains `flag` as its own argument.
bool hasFlag(int argc, char **argv, const char *flag);

/// Check startup and the Enlarge hand-off. No window, no RHI, no single-instance socket.
int runDryRun(int argc, char **argv);

/// Command used to launch Enlarge: `LUCENT_ENLARGE_EXE` when that program exists, else `voxprint-enlarge` on PATH.
QStringList enlargeCliCommand();

/// Page where the user can download Enlarge.
QString enlargeReleasesUrl();

}  // namespace lucent
