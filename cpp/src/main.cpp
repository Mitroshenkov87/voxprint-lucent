// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0
//
// voxprint-lucent: C++/Qt 6 viewer core. Settings, tools, the info panel,
// the Enlarge UI and the updater live in the on-demand PySide6 tools host.

#include "viewer.h"

#include <QApplication>
#include <QFile>
#include <QImageReader>
#include <QLocalServer>
#include <QLocalSocket>

#include <chrono>
#include <cstdio>

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

void takeForward(QLocalSocket *socket, lucent::Viewer &viewer) {
    if (socket == nullptr) {
        return;
    }
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &viewer] {
        const QString path = QString::fromUtf8(socket->readAll()).trimmed();
        if (!path.isEmpty()) {
            viewer.setWindowFilePath(path);
            viewer.openPath(path);
        }
        socket->deleteLater();
    });
}

void acceptForwards(QLocalServer &server, lucent::Viewer &viewer) {
    QObject::connect(&server, &QLocalServer::newConnection, &viewer, [&server, &viewer] {
        takeForward(server.nextPendingConnection(), viewer);
    });
    while (server.hasPendingConnections()) {
        takeForward(server.nextPendingConnection(), viewer);
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

int main(int argc, char **argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto started = std::chrono::steady_clock::now();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Voxprint AI Lucent"));
    QApplication::setOrganizationName(QStringLiteral("Voxprint"));
    QImageReader::setAllocationLimit(512);

    const lucent::LaunchOptions options = parseArgs(QApplication::arguments());
    if (options.image.isEmpty()) {
        std::fprintf(stderr, "usage: voxprint-lucent IMAGE [--benchmark] [--navigate-next IMAGE]\n");
        return 2;
    }
    if (!QFile::exists(options.image)) {
        std::fprintf(stderr, "No such image: %s\n", qPrintable(options.image));
        return 3;
    }
    QLocalServer server;
    if (!options.benchmark && claimOrForward(server, options.image)) {
        return 0;
    }

    lucent::Viewer viewer(options, started);
    if (!options.benchmark && server.isListening()) {
        acceptForwards(server, viewer);
    }
    viewer.show();
    return app.exec();
}
