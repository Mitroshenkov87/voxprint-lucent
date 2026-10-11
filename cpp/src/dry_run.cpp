// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "dry_run.h"

#include "lucent_build.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStandardPaths>

#include <cstdio>
#include <cstring>
#include <string>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace lucent {
namespace {

QString positionalImage(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg == nullptr || arg[0] == '\0') {
            continue;
        }
        if (std::strcmp(arg, "--navigate-next") == 0 && i + 1 < argc) {
            ++i;
            continue;
        }
        if (arg[0] == '-') {
            continue;
        }
        return QString::fromLocal8Bit(arg);
    }
    return {};
}

QJsonValue parseJson(const QByteArray &bytes, bool *ok) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || doc.isNull()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return QJsonValue();
    }
    if (ok != nullptr) {
        *ok = true;
    }
    if (doc.isObject()) {
        return doc.object();
    }
    if (doc.isArray()) {
        return doc.array();
    }
    return QJsonValue();
}

QJsonArray parseLines(const QByteArray &stdoutBytes) {
    QJsonArray lines;
    const QList<QByteArray> rows = stdoutBytes.split('\n');
    for (const QByteArray &row : rows) {
        const QByteArray trimmed = row.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        bool ok = false;
        const QJsonValue value = parseJson(trimmed, &ok);
        if (ok) {
            lines.append(value);
        } else {
            QJsonObject raw;
            raw.insert(QStringLiteral("raw"), QString::fromUtf8(trimmed));
            lines.append(raw);
        }
    }
    return lines;
}

struct CliResult {
    int code = 1;
    QByteArray stdoutBytes;
    bool started = false;
};

CliResult runCli(const QStringList &command, const QStringList &args) {
    CliResult result;
    if (command.isEmpty()) {
        return result;
    }
    QProcess process;
    process.start(command.first(), command.mid(1) + args);
    if (!process.waitForStarted(3000)) {
        return result;
    }
    result.started = true;
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(1000);
        result.code = 1;
        result.stdoutBytes = process.readAllStandardOutput();
        return result;
    }
    result.stdoutBytes = process.readAllStandardOutput();
    result.code = process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1;
    return result;
}

}  // namespace

bool hasFlag(int argc, char **argv, const char *flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

QString enlargeReleasesUrl() {
    return QStringLiteral("https://github.com/Mitroshenkov87/voxprint-enlarge/releases");
}

namespace {

QString voxprintHome() {
    const QString overrideHome = qEnvironmentVariable("VOXPRINT_HOME").trimmed();
    if (!overrideHome.isEmpty()) {
        return overrideHome;
    }
#ifdef Q_OS_WIN
    QString local = qEnvironmentVariable("LOCALAPPDATA").trimmed();
    if (local.isEmpty()) {
        local = QDir::homePath() + QStringLiteral("/AppData/Local");
    }
    return local + QStringLiteral("/Voxprint");
#else
    QString data = qEnvironmentVariable("XDG_DATA_HOME").trimmed();
    if (data.isEmpty()) {
        data = QDir::homePath() + QStringLiteral("/.local/share");
    }
    return data + QStringLiteral("/voxprint");
#endif
}

QString knownEnlargeExe() {
    const QString home = voxprintHome();
#ifdef Q_OS_WIN
    return QDir(home).filePath(QStringLiteral("Enlarge/VoxprintEnlarge.exe"));
#else
    return QDir(home).filePath(QStringLiteral("enlarge/voxprint-enlarge"));
#endif
}

QStringList commandIfFile(const QString &path) {
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    const QFileInfo info(trimmed);
    if (info.isFile()) {
        return {info.absoluteFilePath()};
    }
    const QString found = QStandardPaths::findExecutable(trimmed);
    if (found.isEmpty()) {
        return {};
    }
    return {found};
}

bool manifestHoldsEnlarge(const QJsonObject &root) {
    const QJsonObject components = root.value(QStringLiteral("components")).toObject();
    for (auto it = components.constBegin(); it != components.constEnd(); ++it) {
        const QJsonArray holders = it.value().toObject().value(QStringLiteral("holders")).toArray();
        for (const QJsonValue &holder : holders) {
            if (holder.toString() == QStringLiteral("enlarge")) {
                return true;
            }
        }
    }
    return false;
}

int jsonProtocol(const QJsonValue &value) {
    if (value.isString()) {
        bool ok = false;
        const int parsed = value.toString().toInt(&ok);
        return ok ? parsed : 0;
    }
    return value.toInt(0);
}

QString exeFromManifestValue(const QJsonValue &entry, int *protocol) {
    *protocol = 1;
    if (entry.isString()) {
        return entry.toString();
    }
    if (!entry.isObject()) {
        return {};
    }
    const QJsonObject object = entry.toObject();
    QString exe = object.value(QStringLiteral("exe")).toString();
    if (exe.isEmpty()) {
        exe = object.value(QStringLiteral("Exe")).toString();
    }
    if (object.contains(QStringLiteral("protocol"))) {
        *protocol = jsonProtocol(object.value(QStringLiteral("protocol")));
    } else if (object.contains(QStringLiteral("ProtocolVersion"))) {
        *protocol = jsonProtocol(object.value(QStringLiteral("ProtocolVersion")));
    }
    return exe;
}

QStringList fromSharedManifest() {
    const QString path = QDir(voxprintHome()).filePath(QStringLiteral("shared/manifest.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject apps = root.value(QStringLiteral("apps")).toObject();
    if (apps.contains(QStringLiteral("enlarge"))) {
        int protocol = 1;
        const QString exe = exeFromManifestValue(apps.value(QStringLiteral("enlarge")), &protocol);
        if (protocol == 1) {
            const QStringList command = commandIfFile(exe);
            if (!command.isEmpty()) {
                return command;
            }
        }
    }
    if (manifestHoldsEnlarge(root)) {
        return commandIfFile(knownEnlargeExe());
    }
    return {};
}

#ifdef Q_OS_WIN
QString registryString(HKEY key, const wchar_t *name) {
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ
        || size < sizeof(wchar_t)) {
        return {};
    }
    std::wstring buffer(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS) {
        return {};
    }
    while (!buffer.empty() && buffer.back() == L'\0') {
        buffer.pop_back();
    }
    return QString::fromWCharArray(buffer.c_str());
}

bool registryDword(HKEY key, const wchar_t *name, DWORD *value) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD stored = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<LPBYTE>(&stored), &size) != ERROR_SUCCESS) {
        return false;
    }
    if (type != REG_DWORD) {
        return false;
    }
    *value = stored;
    return true;
}

QStringList fromWindowsRegistry() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Voxprint\\Enlarge", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD protocol = 0;
        const bool haveProtocol = registryDword(key, L"ProtocolVersion", &protocol);
        const QString exe = registryString(key, L"Exe");
        const QString installPath = registryString(key, L"InstallPath");
        RegCloseKey(key);
        if (haveProtocol && protocol == 1 && !exe.isEmpty()) {
            const QString full = QFileInfo(exe).isAbsolute() || installPath.isEmpty()
                ? exe
                : QDir(installPath).filePath(exe);
            const QStringList command = commandIfFile(full);
            if (!command.isEmpty()) {
                return command;
            }
        }
    }
    HKEY appPaths = nullptr;
    const wchar_t *appKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\voxprint-enlarge.exe";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, appKey, 0, KEY_READ, &appPaths) != ERROR_SUCCESS) {
        return {};
    }
    const QString exe = registryString(appPaths, L"");
    RegCloseKey(appPaths);
    return commandIfFile(exe);
}
#endif

QString absoluteImagePath(const QString &image) {
    if (image.isEmpty()) {
        return {};
    }
    return QFileInfo(image).absoluteFilePath();
}

QJsonValue dedupeCapabilityModels(const QJsonValue &value) {
    if (!value.isObject()) {
        return value;
    }
    QJsonObject object = value.toObject();
    const QJsonValue modelsValue = object.value(QStringLiteral("models"));
    if (!modelsValue.isArray()) {
        return value;
    }
    QJsonArray kept;
    QStringList seen;
    const QJsonArray models = modelsValue.toArray();
    for (const QJsonValue &item : models) {
        if (!item.isObject()) {
            kept.append(item);
            continue;
        }
        const QString id = item.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            if (seen.contains(id)) {
                continue;
            }
            seen.append(id);
        }
        kept.append(item);
    }
    object.insert(QStringLiteral("models"), kept);
    return object;
}

}  // namespace

QStringList enlargeCliCommand() {
    const QString raw = qEnvironmentVariable("LUCENT_ENLARGE_EXE").trimmed();
    if (!raw.isEmpty()) {
        const QStringList parts = QProcess::splitCommand(raw);
        if (parts.isEmpty()) {
            return {};
        }
        const QString head = parts.first();
        if (QFileInfo::exists(head) || !QStandardPaths::findExecutable(head).isEmpty()) {
            return parts;
        }
        return {};
    }
#ifdef Q_OS_WIN
    {
        const QStringList registry = fromWindowsRegistry();
        if (!registry.isEmpty()) {
            return registry;
        }
    }
#endif
    {
        const QStringList manifest = fromSharedManifest();
        if (!manifest.isEmpty()) {
            return manifest;
        }
    }
    {
        const QStringList known = commandIfFile(knownEnlargeExe());
        if (!known.isEmpty()) {
            return known;
        }
    }
    const QString found = QStandardPaths::findExecutable(QStringLiteral("voxprint-enlarge"));
    if (!found.isEmpty()) {
        return {found};
    }
#ifdef Q_OS_WIN
    const QString branded = QStandardPaths::findExecutable(QStringLiteral("VoxprintEnlarge.exe"));
    if (!branded.isEmpty()) {
        return {branded};
    }
#endif
    return {};
}

int runDryRun(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString image = absoluteImagePath(positionalImage(argc, argv));
    const bool missing = !image.isEmpty() && !QFileInfo::exists(image);

    QJsonObject root;
    root.insert(QStringLiteral("dry_run"), true);
    root.insert(QStringLiteral("version"), QString::fromUtf8(LUCENT_BUILD_LABEL));
    root.insert(QStringLiteral("startup"), missing ? QStringLiteral("missing_image") : QStringLiteral("ok"));
    if (image.isEmpty()) {
        root.insert(QStringLiteral("image"), QJsonValue::Null);
    } else {
        root.insert(QStringLiteral("image"), image);
    }

    QJsonObject enlarge;
    enlarge.insert(QStringLiteral("transport"), QStringLiteral("cli"));
    const QStringList command = enlargeCliCommand();
    int exitCode = missing ? 3 : 0;
    if (command.isEmpty()) {
        enlarge.insert(QStringLiteral("installed"), false);
        enlarge.insert(QStringLiteral("install_url"), enlargeReleasesUrl());
    } else {
        enlarge.insert(QStringLiteral("installed"), true);
        const CliResult caps = runCli(command, {QStringLiteral("--capabilities"), QStringLiteral("--json")});
        enlarge.insert(QStringLiteral("capabilities_exit"), caps.code);
        bool capsOk = false;
        const QJsonValue capsJson = parseJson(caps.stdoutBytes.trimmed(), &capsOk);
        if (capsOk) {
            enlarge.insert(QStringLiteral("capabilities"), dedupeCapabilityModels(capsJson));
        }
        if (!missing && caps.code != 0) {
            exitCode = caps.code;
        }
        if (!missing && caps.started && caps.code == 0 && !image.isEmpty()) {
            const CliResult job = runCli(
                command,
                {QStringLiteral("--input"), image, QStringLiteral("--scale"), QStringLiteral("2"),
                 QStringLiteral("--preset"), QStringLiteral("photo"), QStringLiteral("--quality-tier"),
                 QStringLiteral("fast"), QStringLiteral("--format"), QStringLiteral("avif"),
                 QStringLiteral("--json-progress"), QStringLiteral("--dry-run")});
            enlarge.insert(QStringLiteral("job_exit"), job.code);
            enlarge.insert(QStringLiteral("lines"), parseLines(job.stdoutBytes));
            if (job.code != 0) {
                exitCode = job.code;
            }
        } else if (!missing && !caps.started) {
            exitCode = 1;
        }
    }
    root.insert(QStringLiteral("enlarge"), enlarge);

    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    return exitCode;
}

}  // namespace lucent
