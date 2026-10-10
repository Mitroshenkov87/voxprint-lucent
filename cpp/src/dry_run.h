// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>
#include <QStringList>

namespace lucent {

bool hasFlag(int argc, char **argv, const char *flag);

// QCoreApplication only. No window, no RHI, no single-instance socket.
int runDryRun(int argc, char **argv);

// LUCENT_ENLARGE_EXE when that program exists, otherwise voxprint-enlarge on PATH.
QStringList enlargeCliCommand();

QString enlargeReleasesUrl();

}  // namespace lucent
