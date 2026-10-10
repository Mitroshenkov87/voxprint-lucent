// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0
//
// voxprint-lucent: C++/Qt 6 viewer core. Settings, tools, the info panel,
// the Enlarge UI and the updater live in the on-demand PySide6 tools host.

#include "browser.h"
#include "dry_run.h"
#include "lucent_build.h"
#include "viewer.h"

#include <QApplication>
#include <QFile>
#include <QImageReader>
#include <QLocalServer>
#include <QLocalSocket>

#include <chrono>
#include <cstdio>
#include <functional>

namespace {

QString serverName() {
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME", QStringLiteral("user")));
    return QStringLiteral("voxprint-lucent-%1").arg(user);
}

// listen() is the fast path. A connect is attempted only when the name is
// already taken, so a first launch does not pay waitForConnected.
bool claimOrForward(QLocalServer &server, const QString &path) {
    const QString name = serverName();
    if (server.listen(name)) {
        return false;
    }
    QLocalSocket socket;
    socket.connectToServer(name);
    if (socket.waitForConnected(200)) {
        socket.write(path.toUtf8() + '\n');
        socket.waitForBytesWritten(200);
        return true;
    }
    QLocalServer::removeServer(name);
    if (!server.listen(name)) {
        std::fprintf(stderr, "single-instance listen failed: %s\n", qPrintable(server.errorString()));
    }
    return false;
}

void takeForward(QLocalSocket *socket, const std::function<void(const QString &)> &open) {
    if (socket == nullptr) {
        return;
    }
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, open] {
        const QString path = QString::fromUtf8(socket->readAll()).trimmed();
        if (!path.isEmpty()) {
            open(path);
        }
        socket->deleteLater();
    });
}

void acceptForwards(QLocalServer &server, const std::function<void(const QString &)> &open) {
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&server, open] {
        while (server.hasPendingConnections()) {
            takeForward(server.nextPendingConnection(), open);
        }
    });
    while (server.hasPendingConnections()) {
        takeForward(server.nextPendingConnection(), open);
    }
}

lucent::LaunchOptions parseArgs(const QStringList &args) {
    lucent::LaunchOptions options;
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (arg == QStringLiteral("--benchmark")) {
            options.benchmark = true;
        } else if (arg == QStringLiteral("--navigate-next") && i + 1 < args.size()) {
            options.navigateNext = args.at(++i);
        } else if (!arg.startsWith(QLatin1Char('-'))) {
            options.image = arg;
        }
    }
    return options;
}

}  // namespace

static void printHelp() {
    std::fputs(
        "voxprint-lucent — image viewer\n"
        "\n"
        "Usage:\n"
        "  voxprint-lucent --help\n"
        "  voxprint-lucent --version\n"
        "  voxprint-lucent --dry-run [IMAGE]\n"
        "  voxprint-lucent --benchmark IMAGE [--navigate-next IMAGE]\n"
        "  voxprint-lucent [IMAGE]\n"
        "\n"
        "--help\n"
        "    Show this text and exit 0. No window is created.\n"
        "\n"
        "--version\n"
        "    Print the build label and exit 0. No window is created.\n"
        "    This flag wins over --dry-run, --benchmark, and an image path.\n"
        "\n"
        "--dry-run [IMAGE]\n"
        "    Check startup and the Enlarge hand-off, print one JSON object, and exit.\n"
        "    No window, no GPU, and no single-instance socket. The tools process is not started.\n"
        "    Exit 0 when IMAGE is omitted or exists and the Enlarge probe succeeds.\n"
        "    Exit 3 when IMAGE does not exist.\n"
        "    Enlarge is LUCENT_ENLARGE_EXE when that program exists, otherwise voxprint-enlarge on PATH.\n"
        "    When Enlarge is absent, enlarge.installed is false and enlarge.install_url is the releases page.\n"
        "    When Enlarge is present, Lucent runs: voxprint-enlarge --capabilities --json\n"
        "    If IMAGE was given and that probe exits 0, Lucent then runs:\n"
        "    voxprint-enlarge --input IMAGE --scale 2 --preset photo --quality-tier fast --format avif --json-progress --dry-run\n"
        "    A non-zero Enlarge status becomes Lucent's status. A missing image still exits 3.\n"
        "    Combined with --benchmark, --dry-run wins and FIRST_PIXEL is not printed.\n"
        "\n"
        "--benchmark IMAGE\n"
        "    Paint IMAGE, print FIRST_PIXEL, and exit. The tools process is not started.\n"
        "    --navigate-next IMAGE opens that second image and prints NAVIGATE_PIXEL.\n"
        "    Exit 2 when IMAGE is missing.\n"
        "\n"
        "IMAGE\n"
        "    Picture to open. A second launch forwards the path to the instance already running.\n"
        "\n"
        "Exit status:\n"
        "    0  the command finished\n"
        "    2  --benchmark was given without an image\n"
        "    3  --dry-run was given a path that is not a file\n",
        stdout);
}

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (lucent::hasFlag(argc, argv, "--help") || lucent::hasFlag(argc, argv, "-h")) {
        printHelp();
        return 0;
    }
    if (lucent::hasFlag(argc, argv, "--version")) {
        std::fputs(LUCENT_BUILD_LABEL, stdout);
        std::fputc('\n', stdout);
        return 0;
    }
    if (lucent::hasFlag(argc, argv, "--dry-run")) {
        return lucent::runDryRun(argc, argv);
    }
    const auto started = std::chrono::steady_clock::now();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Voxprint AI Lucent"));
    QApplication::setOrganizationName(QStringLiteral("Voxprint"));
    QImageReader::setAllocationLimit(512);

    const lucent::LaunchOptions options = parseArgs(QApplication::arguments());
    if (options.benchmark && options.image.isEmpty()) {
        std::fprintf(stderr, "usage: voxprint-lucent IMAGE [--benchmark] [--navigate-next IMAGE]\n");
        return 2;
    }
    if (!options.image.isEmpty() && !QFile::exists(options.image)) {
        std::fprintf(stderr, "No such image: %s\n", qPrintable(options.image));
        return 3;
    }
    QLocalServer server;
    if (!options.benchmark && !options.image.isEmpty() && claimOrForward(server, options.image)) {
        return 0;
    }

    if (options.benchmark) {
        lucent::Viewer viewer(options, started);
        viewer.show();
        return app.exec();
    }

    lucent::Browser browser(options, started);
    if (server.isListening()) {
        acceptForwards(server, [&browser](const QString &path) { browser.openPath(path); });
    }
    browser.show();
    return app.exec();
}
