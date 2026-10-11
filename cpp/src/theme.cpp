// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "theme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace lucent {
namespace {

QString applyTokens(QString sheet, const QJsonObject &tokens) {
    const QString accent = tokens.value(QStringLiteral("ACCENT")).toString();
    sheet.replace(QStringLiteral("@SPLASH@"), accent);
    sheet.replace(QStringLiteral("@TEXT_ON_SELECTION@"), QStringLiteral("#ffffff"));
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        sheet.replace(QStringLiteral("@") + it.key() + QStringLiteral("@"), it.value().toString());
    }
    return sheet;
}

}  // namespace

QString findShareDir() {
    const QString overrideDir = qEnvironmentVariable("LUCENT_SHARE");
    if (!overrideDir.isEmpty() && QFile::exists(overrideDir + QStringLiteral("/tokens.json"))) {
        return overrideDir;
    }
    QDir dir(QCoreApplication::applicationDirPath());
    for (int hop = 0; hop < 6; ++hop) {
        if (QFile::exists(dir.filePath(QStringLiteral("share/tokens.json")))) {
            return dir.filePath(QStringLiteral("share"));
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return {};
}

QString loadStyleSheet() {
    const QString share = findShareDir();
    if (share.isEmpty()) {
        return QStringLiteral("QWidget { background-color: #17171c; color: #f2f2f5; }");
    }
    QFile ready(share + QStringLiteral("/lucent.qss"));
    if (ready.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(ready.readAll());
    }
    QFile tokensFile(share + QStringLiteral("/tokens.json"));
    QFile templateFile(share + QStringLiteral("/lucent.qss.in"));
    if (!tokensFile.open(QIODevice::ReadOnly) || !templateFile.open(QIODevice::ReadOnly)) {
        return QStringLiteral("QWidget { background-color: #17171c; color: #f2f2f5; }");
    }
    const QJsonObject tokens = QJsonDocument::fromJson(tokensFile.readAll()).object();
    return applyTokens(QString::fromUtf8(templateFile.readAll()), tokens);
}

QColor rootPlain() {
    const QString share = findShareDir();
    QFile tokensFile(share + QStringLiteral("/tokens.json"));
    if (!tokensFile.open(QIODevice::ReadOnly)) {
        return QColor(0x17, 0x17, 0x1c);
    }
    const QJsonObject tokens = QJsonDocument::fromJson(tokensFile.readAll()).object();
    const QColor colour(tokens.value(QStringLiteral("ROOT_PLAIN")).toString());
    return colour.isValid() ? colour : QColor(0x17, 0x17, 0x1c);
}

}  // namespace lucent
