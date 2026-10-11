// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "selftest.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>

namespace lucent {
namespace {

struct Check {
    const char *name;
    bool ok;
    bool required;
    QString detail;
};

bool hasFormat(const char *name) {
    return QImageReader::supportedImageFormats().contains(QByteArray(name));
}

QList<Check> collectChecks() {
    QList<Check> checks;
    const bool jpeg = hasFormat("jpeg") || hasFormat("jpg");
    checks.append(Check{"jpeg", jpeg, true,
                         jpeg ? QStringLiteral("The JPEG reader is installed.")
                              : QStringLiteral("The JPEG reader is missing.")});
    const bool webp = hasFormat("webp");
    checks.append(Check{"webp", webp, false,
                         webp ? QStringLiteral("The WebP reader is installed.")
                              : QStringLiteral("This install has no WebP reader. JPEG still opens.")});
    bool avif = hasFormat("avif");
#if defined(LUCENT_HAS_LIBAVIF)
    avif = true;
#endif
    checks.append(Check{"avif", avif, false,
                         avif ? QStringLiteral("AVIF can be opened.")
                              : QStringLiteral("This install has no AVIF reader. JPEG and WebP still open.")});
    bool jxl = hasFormat("jxl");
#if defined(LUCENT_HAS_LIBJXL)
    jxl = true;
#endif
    checks.append(Check{"jpegxl", jxl, false,
                         jxl ? QStringLiteral("JPEG XL can be opened.")
                             : QStringLiteral("This install has no JPEG XL reader. JPEG and WebP still open.")});
#if defined(_WIN32)
    const QDir dir(QCoreApplication::applicationDirPath());
    const bool bundled = QFile::exists(dir.filePath(QStringLiteral("vcruntime140.dll")))
        && QFile::exists(dir.filePath(QStringLiteral("msvcp140.dll")));
    checks.append(Check{
        "vc_runtime", true, true,
        bundled ? QStringLiteral("The Visual C++ runtime is in the install folder.")
                : QStringLiteral("The Visual C++ runtime loaded with Lucent.")});
#else
    checks.append(Check{"vc_runtime", true, false,
                         QStringLiteral("This system does not use the Visual C++ runtime.")});
#endif
    return checks;
}

QJsonDocument documentFor(const QList<Check> &checks) {
    bool ok = true;
    QJsonArray items;
    for (const Check &check : checks) {
        if (check.required && !check.ok) {
            ok = false;
        }
        items.append(QJsonObject{{QStringLiteral("name"), QString::fromLatin1(check.name)},
                                 {QStringLiteral("ok"), check.ok},
                                 {QStringLiteral("required"), check.required},
                                 {QStringLiteral("detail"), check.detail}});
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("ok"), ok}, {QStringLiteral("checks"), items}});
}

}  // namespace

int runSelfTest() {
    const QJsonDocument document = documentFor(collectChecks());
    const QByteArray json = document.toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
    std::fputc('\n', stdout);
    return document.object().value(QStringLiteral("ok")).toBool() ? 0 : 1;
}

QString runtimeWarning() {
    QStringList lines;
    for (const Check &check : collectChecks()) {
        if (check.required && !check.ok) {
            lines.append(check.detail);
        }
    }
    if (lines.isEmpty()) {
        return {};
    }
    lines.prepend(QStringLiteral("Lucent can't show pictures yet."));
    lines.append(QStringLiteral(
        "Reinstall Voxprint AI Lucent. The installer copies these files next to the program."));
    return lines.join(QStringLiteral("\n"));
}

}  // namespace lucent
