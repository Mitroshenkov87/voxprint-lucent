// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <QString>
#include <QStringList>

class QProcess;

namespace lucent {

/// True when `argv` contains `flag` as its own argument.
bool hasFlag(int argc, char **argv, const char *flag);

/// Check startup and the Enlarge hand-off. No window, no RHI, no single-instance socket.
int runDryRun(int argc, char **argv);

/// Command used to launch Enlarge.
///
/// Order: `LUCENT_ENLARGE_EXE`, the HKCU Enlarge key, App Paths (`voxprint-enlarge.exe`,
/// file `VoxprintEnlarge.exe`), the shared suite manifest, the per-user installer path,
/// then `voxprint-enlarge` on PATH.
QStringList enlargeCliCommand();

/// Page where the user can download Enlarge.
QString enlargeReleasesUrl();

/// Byte size from the suite manifest or an MDL catalog, or 0 when neither lists one.
qint64 enlargeListedBytes();

/// Start `voxprint-lucent-tools` with `tail`. The process is configured and not started.
bool startToolsHost(QProcess &process, const QStringList &tail);

}  // namespace lucent
