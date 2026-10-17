// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "labels.h"

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace lucent {
namespace {

QString gLanguage = QStringLiteral("en");
QHash<QString, QString> gShell;
QHash<QString, QString> gShellEn;
QHash<QString, QString> gApp;
QHash<QString, QString> gAppEn;
QString gShare;

QHash<QString, QString> readCatalog(const QString &path) {
    QHash<QString, QString> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isString()) {
            out.insert(it.key(), it.value().toString());
        }
    }
    return out;
}

void reload() {
    gShellEn = readCatalog(gShare + QStringLiteral("/locales/en.json"));
    gAppEn = readCatalog(gShare + QStringLiteral("/locales/lucent/en.json"));
    const QString code = gLanguage;
    gShell = code == QStringLiteral("en") ? gShellEn : readCatalog(gShare + QStringLiteral("/locales/") + code + QStringLiteral(".json"));
    gApp = code == QStringLiteral("en") ? gAppEn
                                        : readCatalog(gShare + QStringLiteral("/locales/lucent/") + code + QStringLiteral(".json"));
}

}  // namespace

void loadCatalogs(const QString &shareDir) {
    gShare = shareDir;
    reload();
}

QString catalogLanguage() { return gLanguage; }

void setCatalogLanguage(const QString &code) {
    const QString trimmed = code.left(2).toLower();
    gLanguage = (trimmed == QStringLiteral("ru") || trimmed == QStringLiteral("de") || trimmed == QStringLiteral("uk")
                 || trimmed == QStringLiteral("lv"))
        ? trimmed
        : QStringLiteral("en");
    reload();
}

QString label(const QString &key) {
    if (gApp.contains(key)) {
        return gApp.value(key);
    }
    if (gShell.contains(key)) {
        return gShell.value(key);
    }
    if (gAppEn.contains(key)) {
        return gAppEn.value(key);
    }
    if (gShellEn.contains(key)) {
        return gShellEn.value(key);
    }
    return key;
}

QString labelWithName(const QString &pattern, const QString &name) {
    QString text = pattern;
    text.replace(QStringLiteral("{name}"), name);
    return text;
}

}  // namespace lucent
