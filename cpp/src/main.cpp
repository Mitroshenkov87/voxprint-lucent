// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0
//
// voxprint-lucent: C++/Qt 6 viewer core. Settings, tools, the info panel,
// the Enlarge UI and the updater live in the on-demand PySide6 tools host.

#include "browser.h"
#include "dry_run.h"
#include "lucent_build.h"
#include "selftest.h"
#include "viewer.h"

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QTimer>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cwchar>
#include <system_error>
#include <thread>
#endif

namespace {

#ifdef Q_OS_WIN
std::wstring lowerWide(std::wstring text) {
    if (!text.empty()) {
        CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
    }
    return text;
}

std::wstring trimSlash(std::wstring text) {
    while (!text.empty() && (text.back() == L'\\' || text.back() == L'/')) {
        text.pop_back();
    }
    return text;
}

// True when this process is the per-machine viewer, not a per-user or dev build.
bool perMachineLucentExe() {
    wchar_t self[MAX_PATH];
    const DWORD selfLen = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (selfLen == 0 || selfLen >= MAX_PATH) {
        return false;
    }
    return lowerWide(self).find(L"\\voxprint ai suite\\lucent\\") != std::wstring::npos;
}

// Registry walk and msiexec live here. CREATE_NEW on the log is the once-only gate.
// Nothing under %LOCALAPPDATA%\Voxprint is deleted by this function.
void removeLegacyPerUserProduct() {
    try {
        if (!perMachineLucentExe()) {
            return;
        }
        wchar_t localApp[MAX_PATH];
        const DWORD localLen = GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
        if (localLen == 0 || localLen >= MAX_PATH) {
            return;
        }
        const std::wstring logDir = std::wstring(localApp) + L"\\Voxprint";
        const std::wstring logPath = logDir + L"\\legacy-uninstall.log";
        const std::wstring legacyDir = lowerWide(trimSlash(logDir + L"\\Lucent"));

        std::wstring product;
        HKEY root = nullptr;
        if (RegOpenKeyExW(
                HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ, &root)
            == ERROR_SUCCESS) {
            for (DWORD index = 0;; ++index) {
                wchar_t name[256];
                DWORD nameLen = 255;
                const LONG enumerated =
                    RegEnumKeyExW(root, index, name, &nameLen, nullptr, nullptr, nullptr, nullptr);
                if (enumerated == ERROR_NO_MORE_ITEMS) {
                    break;
                }
                if (enumerated != ERROR_SUCCESS) {
                    if (enumerated != ERROR_MORE_DATA) {
                        break;
                    }
                    continue;
                }
                name[nameLen] = L'\0';
                if (nameLen < 3 || name[0] != L'{' || name[nameLen - 1] != L'}') {
                    continue;
                }
                HKEY sub = nullptr;
                if (RegOpenKeyExW(root, name, 0, KEY_READ, &sub) != ERROR_SUCCESS) {
                    continue;
                }
                wchar_t display[256];
                DWORD type = 0;
                DWORD size = sizeof(display) - sizeof(wchar_t);
                std::memset(display, 0, sizeof(display));
                const bool haveName =
                    RegQueryValueExW(sub, L"DisplayName", nullptr, &type, reinterpret_cast<LPBYTE>(display), &size)
                        == ERROR_SUCCESS
                    && type == REG_SZ;
                wchar_t location[MAX_PATH];
                type = 0;
                size = sizeof(location) - sizeof(wchar_t);
                std::memset(location, 0, sizeof(location));
                const bool haveLocation =
                    RegQueryValueExW(sub, L"InstallLocation", nullptr, &type, reinterpret_cast<LPBYTE>(location), &size)
                        == ERROR_SUCCESS
                    && type == REG_SZ;
                RegCloseKey(sub);
                const bool oldName = haveName && std::wcscmp(display, L"Voxprint AI Lucent") == 0;
                const bool oldDir = haveLocation && lowerWide(trimSlash(location)) == legacyDir;
                if (oldName || oldDir) {
                    product.assign(name, nameLen);
                    break;
                }
            }
            RegCloseKey(root);
        }

        CreateDirectoryW(logDir.c_str(), nullptr);
        const HANDLE file = CreateFileW(
            logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return;
        }
        std::string line = "no legacy per-user product\r\n";
        if (!product.empty()) {
            wchar_t systemDir[MAX_PATH];
            const UINT systemLen = GetSystemDirectoryW(systemDir, MAX_PATH);
            std::wstring command = (systemLen > 0 && systemLen < MAX_PATH)
                ? std::wstring(systemDir) + L"\\msiexec.exe /x "
                : std::wstring(L"msiexec.exe /x ");
            command += product + L" /qn /norestart";
            std::vector<wchar_t> mutableCommand(command.begin(), command.end());
            mutableCommand.push_back(L'\0');
            STARTUPINFOW startup;
            std::memset(&startup, 0, sizeof(startup));
            startup.cb = sizeof(startup);
            startup.dwFlags = STARTF_USESHOWWINDOW;
            startup.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION process;
            std::memset(&process, 0, sizeof(process));
            const BOOL started = CreateProcessW(
                nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &startup, &process);
            if (started) {
                CloseHandle(process.hThread);
                CloseHandle(process.hProcess);
                line = "spawned msiexec /x ";
                for (wchar_t ch : product) {
                    line.push_back(static_cast<char>(ch));
                }
                line += " /qn\r\n";
            } else {
                line = "msiexec spawn failed\r\n";
            }
        }
        DWORD written = 0;
        WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        CloseHandle(file);
    } catch (...) {
        // The viewer must open even when this uninstall cannot run.
    }
}

// Returns as soon as the background thread is queued. A later launch sees the log and does nothing.
void spawnLegacyPerUserUninstall() {
    if (!perMachineLucentExe()) {
        return;
    }
    wchar_t localApp[MAX_PATH];
    const DWORD localLen = GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH);
    if (localLen == 0 || localLen >= MAX_PATH) {
        return;
    }
    const std::wstring logPath = std::wstring(localApp) + L"\\Voxprint\\legacy-uninstall.log";
    if (GetFileAttributesW(logPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    try {
        std::thread(removeLegacyPerUserProduct).detach();
    } catch (const std::system_error &) {
    }
}
#endif

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
        "  voxprint-lucent --selftest --json\n"
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
        "    Enlarge is found in this order: LUCENT_ENLARGE_EXE, HKLM\\Software\\VOXPrint\\Enlarge,\n"
        "    HKCU\\Software\\VOXPrint\\Enlarge, App Paths, Program Files\\VOXPrint AI Suite\\Enlarge,\n"
        "    the shared suite manifest, the per-user installer path, then voxprint-enlarge on PATH.\n"
        "    --input is always an absolute path.\n"
        "    When Enlarge is absent, enlarge.installed is false and enlarge.install_url is the releases page.\n"
        "    When Enlarge is present, Lucent runs: voxprint-enlarge --capabilities --json\n"
        "    If IMAGE was given and that probe exits 0, Lucent then runs:\n"
        "    voxprint-enlarge --input IMAGE --scale 2 --preset photo --quality-tier fast --format avif --json-progress --dry-run\n"
        "    A non-zero Enlarge status becomes Lucent's status. A missing image still exits 3.\n"
        "    Combined with --benchmark, --dry-run wins and FIRST_PIXEL is not printed.\n"
        "\n"
        "--selftest --json\n"
        "    Print one JSON object naming the JPEG, WebP, AVIF, JPEG XL, and Visual C++\n"
        "    runtime checks, then exit. No window is created. JPEG is required.\n"
        "    WebP, AVIF, and JPEG XL are reported and do not fail this command when\n"
        "    the install does not include them. Exit 1 when a required check fails.\n"
        "\n"
        "--benchmark IMAGE\n"
        "    Paint IMAGE, print FIRST_PIXEL, and exit. The tools process is not started.\n"
        "    On Windows session 0 (no interactive desktop) this exits with an error.\n"
        "    A paint that has not finished in 60 seconds exits with an error.\n"
        "    --navigate-next IMAGE opens that second image and prints NAVIGATE_PIXEL.\n"
        "    Exit 2 when IMAGE is missing.\n"
        "\n"
        "IMAGE\n"
        "    Picture to open. A second launch forwards the path to the instance already running.\n"
        "\n"
        "Exit status:\n"
        "    0  the command finished\n"
        "    2  --benchmark was given without an image\n"
        "    3  --dry-run was given a path that is not a file\n"
        "    1  --selftest found a required file, or --benchmark is session 0 or timed out\n",
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
    if (lucent::hasFlag(argc, argv, "--selftest")) {
        QGuiApplication probe(argc, argv);
        return lucent::runSelfTest();
    }
#ifdef Q_OS_WIN
    if (lucent::hasFlag(argc, argv, "--benchmark")) {
        DWORD sessionId = 0;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) || sessionId == 0) {
            std::fprintf(stderr, "voxprint-lucent: --benchmark needs an interactive desktop (session 0)\n");
            return 1;
        }
    }
#endif
    const auto started = std::chrono::steady_clock::now();
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("VOXPrint AI Lucent"));
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

    const QString warning = lucent::runtimeWarning();
    if (!warning.isEmpty()) {
        std::fprintf(stderr, "%s\n", qPrintable(warning));
        QMessageBox::critical(nullptr, QStringLiteral("VOXPrint AI Lucent"), warning);
        return 1;
    }

    if (options.benchmark) {
        QTimer::singleShot(60000, &app, [] {
            std::fprintf(stderr, "voxprint-lucent: --benchmark timed out after 60 s\n");
            QCoreApplication::exit(1);
        });
        lucent::Viewer viewer(options, started);
        viewer.show();
        return app.exec();
    }

    lucent::Browser browser(options, started);
    if (server.isListening()) {
        acceptForwards(server, [&browser](const QString &path) { browser.openPath(path); });
    }
    browser.show();
#ifdef Q_OS_WIN
    QTimer::singleShot(0, &app, [] { spawnLegacyPerUserUninstall(); });
#endif
    return app.exec();
}
