// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0
//
// Console entry. The viewer binary is a Windows application, so a terminal
// does not own its stdout. This process starts that binary and waits.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

std::wstring quoteArg(const std::wstring &arg) {
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    unsigned backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, L'\\');
        backslashes = 0;
        out.push_back(ch);
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

}  // namespace

int main() {
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr || argc < 1) {
        std::fwprintf(stderr, L"voxprint-lucent-cli: no command line\n");
        return 1;
    }
    wchar_t self[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        std::fwprintf(stderr, L"voxprint-lucent-cli: cannot locate this executable\n");
        LocalFree(argv);
        return 1;
    }
    std::wstring path(self, length);
    const std::size_t slash = path.find_last_of(L"\\/");
    const std::wstring gui = path.substr(0, slash) + L"\\voxprint-lucent.exe";

    std::wstring command = quoteArg(gui);
    for (int i = 1; i < argc; ++i) {
        command.push_back(L' ');
        command += quoteArg(argv[i]);
    }
    LocalFree(argv);

    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup;
    std::memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process;
    std::memset(&process, 0, sizeof(process));
    if (!CreateProcessW(
            gui.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
            &process)) {
        std::fwprintf(stderr, L"voxprint-lucent-cli: cannot start %s (%lu)\n", gui.c_str(), GetLastError());
        return 1;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
}
