// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "dry_run.h"

#include "lucent_build.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QStandardPaths>

#include <cstdio>
#include <cstring>

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
    const QString found = QStandardPaths::findExecutable(QStringLiteral("voxprint-enlarge"));
    if (found.isEmpty()) {
        return {};
    }
    return {found};
}

int runDryRun(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QString image = positionalImage(argc, argv);
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
            enlarge.insert(QStringLiteral("capabilities"), capsJson);
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
