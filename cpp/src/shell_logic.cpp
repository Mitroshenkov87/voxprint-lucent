// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "shell_logic.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lucent {
namespace {

struct GoodCache {
    std::mutex mutex;
    std::map<std::string, Json> documents;
    std::map<std::string, std::pair<std::int64_t, std::uintmax_t>> tokens;
};

GoodCache &cache() {
    static GoodCache store;
    return store;
}

std::string cacheKey(const std::filesystem::path &path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return (error ? path : absolute).generic_string();
}

void skipSpace(const std::string &text, std::size_t &index) {
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t' || text[index] == '\n' || text[index] == '\r')) {
        ++index;
    }
}

bool parseValue(const std::string &text, std::size_t &index, Json &out, std::string &error);

bool parseString(const std::string &text, std::size_t &index, std::string &out, std::string &error) {
    if (index >= text.size() || text[index] != '"') {
        error = "expected a string";
        return false;
    }
    ++index;
    out.clear();
    while (index < text.size()) {
        const char ch = text[index++];
        if (ch == '"') {
            return true;
        }
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (index >= text.size()) {
            error = "bad string escape";
            return false;
        }
        const char escaped = text[index++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            out.push_back(escaped);
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'u': {
            if (index + 4 > text.size()) {
                error = "bad unicode escape";
                return false;
            }
            int code = 0;
            for (int nibble = 0; nibble < 4; ++nibble) {
                const char hex = text[index++];
                code <<= 4;
                if (hex >= '0' && hex <= '9') {
                    code += hex - '0';
                } else if (hex >= 'a' && hex <= 'f') {
                    code += hex - 'a' + 10;
                } else if (hex >= 'A' && hex <= 'F') {
                    code += hex - 'A' + 10;
                } else {
                    error = "bad unicode escape";
                    return false;
                }
            }
            if (code < 0x80) {
                out.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            break;
        }
        default:
            error = "bad string escape";
            return false;
        }
    }
    error = "unterminated string";
    return false;
}

bool parseNumber(const std::string &text, std::size_t &index, Json &out, std::string &error) {
    const std::size_t start = index;
    if (text[index] == '-') {
        ++index;
    }
    if (index >= text.size() || text[index] < '0' || text[index] > '9') {
        error = "bad number";
        return false;
    }
    if (text[index] == '0') {
        ++index;
    } else {
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            ++index;
        }
    }
    bool integral = true;
    if (index < text.size() && text[index] == '.') {
        integral = false;
        ++index;
        if (index >= text.size() || text[index] < '0' || text[index] > '9') {
            error = "bad number";
            return false;
        }
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            ++index;
        }
    }
    if (index < text.size() && (text[index] == 'e' || text[index] == 'E')) {
        integral = false;
        ++index;
        if (index < text.size() && (text[index] == '+' || text[index] == '-')) {
            ++index;
        }
        if (index >= text.size() || text[index] < '0' || text[index] > '9') {
            error = "bad number";
            return false;
        }
        while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
            ++index;
        }
    }
    const std::string token = text.substr(start, index - start);
    if (integral) {
        try {
            std::size_t used = 0;
            const long long value = std::stoll(token, &used, 10);
            if (used != token.size()) {
                error = "bad number";
                return false;
            }
            out = Json::fromInt(value);
            return true;
        } catch (const std::exception &) {
            error = "bad number";
            return false;
        }
    }
    try {
        std::size_t used = 0;
        const double value = std::stod(token, &used);
        if (used != token.size()) {
            error = "bad number";
            return false;
        }
        out = Json::fromFloat(value);
        return true;
    } catch (const std::exception &) {
        error = "bad number";
        return false;
    }
}

bool parseValue(const std::string &text, std::size_t &index, Json &out, std::string &error) {
    skipSpace(text, index);
    if (index >= text.size()) {
        error = "unexpected end";
        return false;
    }
    const char ch = text[index];
    if (ch == '"') {
        std::string value;
        if (!parseString(text, index, value, error)) {
            return false;
        }
        out = Json::fromString(std::move(value));
        return true;
    }
    if (ch == '-' || (ch >= '0' && ch <= '9')) {
        return parseNumber(text, index, out, error);
    }
    if (text.compare(index, 4, "true") == 0) {
        index += 4;
        out = Json::fromBool(true);
        return true;
    }
    if (text.compare(index, 5, "false") == 0) {
        index += 5;
        out = Json::fromBool(false);
        return true;
    }
    if (text.compare(index, 4, "null") == 0) {
        index += 4;
        out = Json::nul();
        return true;
    }
    if (ch == '[') {
        ++index;
        out = Json();
        out.kind = Json::Kind::Array;
        skipSpace(text, index);
        if (index < text.size() && text[index] == ']') {
            ++index;
            return true;
        }
        while (index < text.size()) {
            Json item;
            if (!parseValue(text, index, item, error)) {
                return false;
            }
            out.array.push_back(std::move(item));
            skipSpace(text, index);
            if (index < text.size() && text[index] == ',') {
                ++index;
                continue;
            }
            if (index < text.size() && text[index] == ']') {
                ++index;
                return true;
            }
            error = "expected ',' or ']'";
            return false;
        }
        error = "unterminated array";
        return false;
    }
    if (ch == '{') {
        ++index;
        out = Json();
        out.kind = Json::Kind::Object;
        skipSpace(text, index);
        if (index < text.size() && text[index] == '}') {
            ++index;
            return true;
        }
        while (index < text.size()) {
            skipSpace(text, index);
            std::string key;
            if (!parseString(text, index, key, error)) {
                return false;
            }
            skipSpace(text, index);
            if (index >= text.size() || text[index] != ':') {
                error = "expected ':'";
                return false;
            }
            ++index;
            Json item;
            if (!parseValue(text, index, item, error)) {
                return false;
            }
            out.set(key, std::move(item));
            skipSpace(text, index);
            if (index < text.size() && text[index] == ',') {
                ++index;
                continue;
            }
            if (index < text.size() && text[index] == '}') {
                ++index;
                return true;
            }
            error = "expected ',' or '}'";
            return false;
        }
        error = "unterminated object";
        return false;
    }
    error = "unexpected token";
    return false;
}

void dumpString(std::ostream &out, const std::string &text) {
    out << '"';
    for (unsigned char ch : text) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (ch < 0x20) {
                const char *hex = "0123456789abcdef";
                out << "\\u00" << hex[ch >> 4] << hex[ch & 0xF];
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    out << '"';
}

void dumpInto(std::ostream &out, const Json &value) {
    switch (value.kind) {
    case Json::Kind::Null:
        out << "null";
        break;
    case Json::Kind::Bool:
        out << (value.boolean ? "true" : "false");
        break;
    case Json::Kind::Int:
        out << value.integer;
        break;
    case Json::Kind::Float:
        out << value.number;
        break;
    case Json::Kind::String:
        dumpString(out, value.text);
        break;
    case Json::Kind::Array: {
        out << '[';
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            if (i != 0) {
                out << ',';
            }
            dumpInto(out, value.array[i]);
        }
        out << ']';
        break;
    }
    case Json::Kind::Object: {
        out << '{';
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            if (i != 0) {
                out << ',';
            }
            dumpString(out, value.object[i].first);
            out << ':';
            dumpInto(out, value.object[i].second);
        }
        out << '}';
        break;
    }
    }
}

std::string lowerCopy(std::string text) {
    for (char &ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

std::string finalSegment(const std::string &key) {
    const auto pos = key.rfind('.');
    if (pos == std::string::npos) {
        return key;
    }
    return key.substr(pos + 1);
}

bool segmentIs(const std::string &segment, const char *word) {
    return lowerCopy(segment) == word;
}

const Json *objectField(const Json &object, const std::string &key) {
    if (object.kind != Json::Kind::Object) {
        return nullptr;
    }
    for (const auto &item : object.object) {
        if (item.first == key) {
            return &item.second;
        }
    }
    return nullptr;
}

bool requireText(const Json &object, const std::string &key, std::string &out, std::string &error) {
    const Json *field = objectField(object, key);
    if (field == nullptr || field->kind != Json::Kind::String || field->text.empty()) {
        error = "missing " + key;
        return false;
    }
    out = field->text;
    return true;
}

bool valueAllowed(const Json &value) {
    return value.kind == Json::Kind::Bool || value.kind == Json::Kind::Int || value.kind == Json::Kind::Float
        || value.kind == Json::Kind::String;
}

std::vector<std::string> optionKeys(const PresetOption &option) {
    std::vector<std::string> keys;
    keys.reserve(option.values.size());
    for (const auto &item : option.values) {
        keys.push_back(item.first);
    }
    return keys;
}

int distanceOf(const PresetOption &option, const std::vector<std::string> &owned, const SettingState &state) {
    int distance = 0;
    for (const std::string &key : owned) {
        const Json *preset = nullptr;
        for (const auto &item : option.values) {
            if (item.first == key) {
                preset = &item.second;
                break;
            }
        }
        const auto live = state.values.find(key);
        if (preset == nullptr || live == state.values.end() || !sameValue(live->second, *preset)) {
            ++distance;
        }
    }
    return distance;
}

void recompute(const PresetFile &file, SettingState &state) {
    for (const PresetGroup &group : file.groups) {
        const ChipStatus status = chipFor(file, state, group.id);
        state.chosen[group.id] = status.optionId;
        if (status.kind == ChipKind::Exact) {
            state.overrides[group.id].clear();
            continue;
        }
        const PresetOption *option = findOption(group, status.optionId);
        std::vector<std::string> changed;
        if (option != nullptr) {
            for (const std::string &key : ownedKeys(file, group.id)) {
                const Json *preset = nullptr;
                for (const auto &item : option->values) {
                    if (item.first == key) {
                        preset = &item.second;
                        break;
                    }
                }
                const auto live = state.values.find(key);
                if (preset == nullptr || live == state.values.end() || !sameValue(live->second, *preset)) {
                    changed.push_back(key);
                }
            }
        }
        state.overrides[group.id] = std::move(changed);
    }
}

bool applyStored(const PresetFile &file, const Json &stored, SettingState &state) {
    if (stored.kind != Json::Kind::Object) {
        return false;
    }
    const Json *schema = objectField(stored, "schema");
    if (schema == nullptr || schema->kind != Json::Kind::Int || schema->integer != 1) {
        return false;
    }
    const Json *values = objectField(stored, "values");
    const Json *chosen = objectField(stored, "chosen");
    if (values == nullptr || values->kind != Json::Kind::Object || chosen == nullptr || chosen->kind != Json::Kind::Object) {
        return false;
    }
    for (const auto &item : values->object) {
        if (reservedSettingKey(item.first) || !valueAllowed(item.second)) {
            return false;
        }
    }
    for (const PresetGroup &group : file.groups) {
        const Json *optionId = objectField(*chosen, group.id);
        if (optionId == nullptr || optionId->kind != Json::Kind::String || findOption(group, optionId->text) == nullptr) {
            return false;
        }
    }
    const Json *overrides = objectField(stored, "overrides");
    if (overrides != nullptr && overrides->kind == Json::Kind::Object) {
        for (const auto &item : overrides->object) {
            if (item.second.kind != Json::Kind::Array) {
                continue;
            }
            for (const Json &key : item.second.array) {
                if (key.kind == Json::Kind::String && reservedSettingKey(key.text)) {
                    return false;
                }
            }
        }
    }
    for (const auto &item : values->object) {
        state.values[item.first] = item.second;
    }
    for (const PresetGroup &group : file.groups) {
        state.chosen[group.id] = objectField(*chosen, group.id)->text;
    }
    return true;
}

#ifdef _WIN32
class FileLock {
public:
    explicit FileLock(const std::filesystem::path &path) {
        const std::filesystem::path lock = std::filesystem::path(path.wstring() + L".lock");
        std::error_code error;
        std::filesystem::create_directories(lock.parent_path(), error);
        handle_ = CreateFileW(
            lock.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            return;
        }
        OVERLAPPED overlapped;
        std::memset(&overlapped, 0, sizeof(overlapped));
        if (!LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    ~FileLock() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            OVERLAPPED overlapped;
            std::memset(&overlapped, 0, sizeof(overlapped));
            UnlockFileEx(handle_, 0, 1, 0, &overlapped);
            CloseHandle(handle_);
        }
    }

    bool ok() const { return handle_ != INVALID_HANDLE_VALUE; }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

bool atomicReplace(const std::filesystem::path &path, const std::string &bytes, std::string &error) {
    std::error_code mkdirError;
    std::filesystem::create_directories(path.parent_path(), mkdirError);
    const std::wstring temporary = path.wstring() + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    HANDLE file = CreateFileW(
        temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = "could not create the temporary file";
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    if (!ok || written != bytes.size() || !FlushFileBuffers(file)) {
        CloseHandle(file);
        DeleteFileW(temporary.c_str());
        error = "could not finish the temporary file";
        return false;
    }
    CloseHandle(file);
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        error = "could not replace the file";
        return false;
    }
    return true;
}
#else
class FileLock {
public:
    explicit FileLock(const std::filesystem::path &path) {
        const std::filesystem::path lock = path.string() + ".lock";
        std::error_code error;
        std::filesystem::create_directories(lock.parent_path(), error);
        fd_ = ::open(lock.c_str(), O_CREAT | O_RDWR, 0644);
        if (fd_ < 0) {
            return;
        }
        if (::lseek(fd_, 0, SEEK_END) == 0) {
            const char mark = '\0';
            if (::write(fd_, &mark, 1) != 1) {
                ::close(fd_);
                fd_ = -1;
                return;
            }
        }
        ::lseek(fd_, 0, SEEK_SET);
        if (::flock(fd_, LOCK_EX) != 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    ~FileLock() {
        if (fd_ >= 0) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
        }
    }

    bool ok() const { return fd_ >= 0; }

private:
    int fd_ = -1;
};

bool atomicReplace(const std::filesystem::path &path, const std::string &bytes, std::string &error) {
    std::error_code mkdirError;
    std::filesystem::create_directories(path.parent_path(), mkdirError);
    std::mt19937_64 rng{std::random_device{}()};
    const std::filesystem::path temporary =
        path.parent_path() / ("." + path.filename().string() + "." + std::to_string(::getpid()) + "." + std::to_string(rng()) + ".tmp");
    const int fd = ::open(temporary.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        error = "could not create the temporary file";
        return false;
    }
    std::size_t written = 0;
    while (written < bytes.size()) {
        const ssize_t step = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (step < 0) {
            ::close(fd);
            ::unlink(temporary.c_str());
            error = "could not write the temporary file";
            return false;
        }
        written += static_cast<std::size_t>(step);
    }
    if (::fsync(fd) != 0) {
        ::close(fd);
        ::unlink(temporary.c_str());
        error = "could not fsync the temporary file";
        return false;
    }
    ::close(fd);
    if (::rename(temporary.c_str(), path.c_str()) != 0) {
        ::unlink(temporary.c_str());
        error = "could not replace the file";
        return false;
    }
    const int directory = ::open(path.parent_path().c_str(), O_RDONLY);
    if (directory >= 0) {
        ::fsync(directory);
        ::close(directory);
    }
    return true;
}
#endif

Json emptyObject() {
    Json object;
    object.kind = Json::Kind::Object;
    return object;
}

void remember(const std::filesystem::path &path, const Json &document, std::int64_t mtime, std::uintmax_t size) {
    auto &store = cache();
    const std::string key = cacheKey(path);
    store.documents[key] = document;
    store.tokens[key] = {mtime, size};
}

}  // namespace

Json Json::nul() { return Json(); }

Json Json::fromBool(bool value) {
    Json json;
    json.kind = Kind::Bool;
    json.boolean = value;
    return json;
}

Json Json::fromInt(long long value) {
    Json json;
    json.kind = Kind::Int;
    json.integer = value;
    json.number = static_cast<double>(value);
    return json;
}

Json Json::fromFloat(double value) {
    Json json;
    json.kind = Kind::Float;
    json.number = value;
    return json;
}

Json Json::fromString(std::string value) {
    Json json;
    json.kind = Kind::String;
    json.text = std::move(value);
    return json;
}

const Json *Json::field(const std::string &key) const { return objectField(*this, key); }

void Json::set(const std::string &key, Json value) {
    kind = Kind::Object;
    for (auto &item : object) {
        if (item.first == key) {
            item.second = std::move(value);
            return;
        }
    }
    object.emplace_back(key, std::move(value));
}

bool Json::isNumber() const { return kind == Kind::Int || kind == Kind::Float; }

double Json::asNumber() const { return kind == Kind::Int ? static_cast<double>(integer) : number; }

bool parseJson(const std::string &text, Json &out, std::string &error) {
    std::size_t index = 0;
    if (!parseValue(text, index, out, error)) {
        return false;
    }
    skipSpace(text, index);
    if (index != text.size()) {
        error = "trailing data";
        return false;
    }
    return true;
}

std::string dumpJson(const Json &value) {
    std::ostringstream out;
    dumpInto(out, value);
    return out.str();
}

std::string readFile(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool sameValue(const Json &left, const Json &right) {
    if (left.kind == Json::Kind::Bool || right.kind == Json::Kind::Bool) {
        return left.kind == Json::Kind::Bool && right.kind == Json::Kind::Bool && left.boolean == right.boolean;
    }
    if (left.isNumber() && right.isNumber()) {
        return std::abs(left.asNumber() - right.asNumber()) < 1e-6;
    }
    return left.kind == Json::Kind::String && right.kind == Json::Kind::String && left.text == right.text;
}

bool reservedSettingKey(const std::string &key) {
    const std::string folded = lowerCopy(key);
    if (folded == "safety" || folded.rfind("safety.", 0) == 0) {
        return true;
    }
    std::string segment;
    for (std::size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '.') {
            if (segmentIs(segment, "safety")) {
                return true;
            }
            segment.clear();
        } else {
            segment.push_back(key[i]);
        }
    }
    const std::string last = lowerCopy(finalSegment(key));
    return last == "minor" || last == "minors" || last == "age" || last == "guard";
}

bool loadPresetDocument(const Json &document, PresetFile &out, std::string &error) {
    out = {};
    const Json *schema = objectField(document, "schema");
    if (schema == nullptr || schema->kind != Json::Kind::String || schema->text != "voxprint-presets/1") {
        error = "presets schema must be voxprint-presets/1";
        return false;
    }
    const Json *groups = objectField(document, "groups");
    if (groups == nullptr || groups->kind != Json::Kind::Array || groups->array.empty()) {
        error = "presets need at least one group";
        return false;
    }
    for (const Json &raw : groups->array) {
        if (raw.kind != Json::Kind::Object) {
            error = "a preset group must be an object";
            return false;
        }
        PresetGroup group;
        if (!requireText(raw, "id", group.id, error) || !requireText(raw, "label_key", group.labelKey, error)
            || !requireText(raw, "default", group.defaultId, error)) {
            return false;
        }
        const Json *options = objectField(raw, "options");
        if (options == nullptr || options->kind != Json::Kind::Array || options->array.empty()) {
            error = "preset group " + group.id + " needs options";
            return false;
        }
        for (const Json &item : options->array) {
            PresetOption option;
            if (item.kind != Json::Kind::Object || !requireText(item, "id", option.id, error)
                || !requireText(item, "label_key", option.labelKey, error)) {
                return false;
            }
            const Json *available = objectField(item, "available");
            option.available = available == nullptr || (available->kind == Json::Kind::Bool && available->boolean);
            if (available != nullptr && available->kind != Json::Kind::Bool) {
                error = "preset option " + option.id + " has a bad availability";
                return false;
            }
            if (const Json *reason = objectField(item, "reason_key"); reason != nullptr && reason->kind == Json::Kind::String) {
                option.reasonKey = reason->text;
            }
            if (const Json *action = objectField(item, "action_label_key");
                action != nullptr && action->kind == Json::Kind::String) {
                option.actionLabelKey = action->text;
            }
            if (const Json *actionId = objectField(item, "action_id");
                actionId != nullptr && actionId->kind == Json::Kind::String) {
                option.actionId = actionId->text;
            }
            if (!option.available && (option.reasonKey.empty() || option.actionLabelKey.empty() || option.actionId.empty())) {
                error = "preset option " + option.id + " needs a plain reason and an action";
                return false;
            }
            const Json *values = objectField(item, "values");
            if (values == nullptr || values->kind != Json::Kind::Object || values->object.empty()) {
                error = "preset option " + option.id + " needs values";
                return false;
            }
            for (const auto &value : values->object) {
                if (value.first.empty() || reservedSettingKey(value.first) || !valueAllowed(value.second)) {
                    error = reservedSettingKey(value.first) ? "reserved key " + value.first
                                                            : "preset option " + option.id + " has a bad value";
                    return false;
                }
                option.values.push_back(value);
            }
            group.options.push_back(std::move(option));
        }
        std::vector<std::string> ids;
        for (const PresetOption &option : group.options) {
            if (std::find(ids.begin(), ids.end(), option.id) != ids.end()) {
                error = "preset group " + group.id + " repeats an option";
                return false;
            }
            ids.push_back(option.id);
        }
        const std::vector<std::string> keys = optionKeys(group.options.front());
        for (const PresetOption &option : group.options) {
            const std::vector<std::string> itemKeys = optionKeys(option);
            if (itemKeys.size() != keys.size()
                || !std::is_permutation(itemKeys.begin(), itemKeys.end(), keys.begin())) {
                error = "preset group " + group.id + " options must write the same keys";
                return false;
            }
        }
        if (std::find(ids.begin(), ids.end(), group.defaultId) == ids.end()) {
            error = "preset group " + group.id + " default is not an option";
            return false;
        }
        for (const PresetGroup &prior : out.groups) {
            if (prior.id == group.id) {
                error = "preset group " + group.id + " is repeated";
                return false;
            }
        }
        out.groups.push_back(std::move(group));
    }
    return true;
}

const PresetGroup *findGroup(const PresetFile &file, const std::string &groupId) {
    for (const PresetGroup &group : file.groups) {
        if (group.id == groupId) {
            return &group;
        }
    }
    return nullptr;
}

const PresetOption *findOption(const PresetGroup &group, const std::string &optionId) {
    for (const PresetOption &option : group.options) {
        if (option.id == optionId) {
            return &option;
        }
    }
    return nullptr;
}

bool setOptionAvailable(PresetFile &file, const std::string &groupId, const std::string &optionId, bool available) {
    for (PresetGroup &group : file.groups) {
        if (group.id != groupId) {
            continue;
        }
        for (PresetOption &option : group.options) {
            if (option.id != optionId) {
                continue;
            }
            if (!available && (option.reasonKey.empty() || option.actionLabelKey.empty() || option.actionId.empty())) {
                return false;
            }
            option.available = available;
            return true;
        }
    }
    return false;
}

SettingState mergePresets(const PresetFile &file, const std::map<std::string, Json> &defaults, const Json *stored) {
    SettingState state;
    state.values = defaults;
    for (const PresetGroup &group : file.groups) {
        state.chosen[group.id] = group.defaultId;
        state.overrides[group.id] = {};
        const PresetOption *option = findOption(group, group.defaultId);
        if (option == nullptr || !option->available) {
            continue;
        }
        for (const auto &item : option->values) {
            state.values[item.first] = item.second;
        }
    }
    if (stored != nullptr) {
        SettingState loaded = state;
        if (applyStored(file, *stored, loaded)) {
            state = std::move(loaded);
        }
    }
    recompute(file, state);
    return state;
}

ChipStatus chipFor(const PresetFile &file, const SettingState &state, const std::string &groupId) {
    ChipStatus status;
    const PresetGroup *group = findGroup(file, groupId);
    if (group == nullptr) {
        status.kind = ChipKind::Custom;
        return status;
    }
    const std::vector<std::string> owned = ownedKeys(file, groupId);
    for (const PresetOption &option : group->options) {
        if (option.available && distanceOf(option, owned, state) == 0) {
            status.kind = ChipKind::Exact;
            status.optionId = option.id;
            status.labelKey = option.labelKey;
            return status;
        }
    }
    std::string storedId = group->defaultId;
    const auto chosen = state.chosen.find(groupId);
    if (chosen != state.chosen.end()) {
        storedId = chosen->second;
    }
    const PresetOption *stored = findOption(*group, storedId);
    if (stored == nullptr) {
        stored = findOption(*group, group->defaultId);
    }
    if (stored == nullptr) {
        status.kind = ChipKind::Custom;
        return status;
    }
    status.optionId = stored->id;
    status.labelKey = stored->labelKey;
    const int distance = distanceOf(*stored, owned, state);
    const int limit = static_cast<int>(owned.size() / 2);
    status.kind = distance > 0 && distance <= limit ? ChipKind::Modified : ChipKind::Custom;
    return status;
}

std::string formatChip(ChipKind kind, const std::string &name, const std::string &changedPattern, const std::string &customWord) {
    if (kind == ChipKind::Exact) {
        return name;
    }
    if (kind == ChipKind::Modified) {
        std::string label = changedPattern;
        const std::string token = "{name}";
        const auto pos = label.find(token);
        if (pos != std::string::npos) {
            label.replace(pos, token.size(), name);
        }
        return label;
    }
    return customWord;
}

bool chooseOption(const PresetFile &file, SettingState &state, const std::string &groupId, const std::string &optionId) {
    const PresetGroup *group = findGroup(file, groupId);
    const PresetOption *option = group == nullptr ? nullptr : findOption(*group, optionId);
    if (group == nullptr || option == nullptr || !option->available) {
        return false;
    }
    for (const auto &item : option->values) {
        if (reservedSettingKey(item.first)) {
            return false;
        }
        state.values[item.first] = item.second;
    }
    state.chosen[groupId] = optionId;
    state.overrides[groupId].clear();
    recompute(file, state);
    return true;
}

bool editValue(const PresetFile &file, SettingState &state, const std::string &key, const Json &value) {
    if (reservedSettingKey(key) || !valueAllowed(value)) {
        return false;
    }
    state.values[key] = value;
    recompute(file, state);
    return true;
}

bool resetGroup(const PresetFile &file, SettingState &state, const std::string &groupId) {
    const PresetGroup *group = findGroup(file, groupId);
    if (group == nullptr) {
        return false;
    }
    const auto chosen = state.chosen.find(groupId);
    const std::string optionId = chosen == state.chosen.end() ? group->defaultId : chosen->second;
    const PresetOption *option = findOption(*group, optionId);
    if (option == nullptr) {
        return false;
    }
    for (const auto &item : option->values) {
        state.values[item.first] = item.second;
    }
    state.overrides[groupId].clear();
    recompute(file, state);
    return true;
}

std::vector<std::string> ownedKeys(const PresetFile &file, const std::string &groupId) {
    const PresetGroup *group = findGroup(file, groupId);
    if (group == nullptr || group->options.empty()) {
        return {};
    }
    std::vector<std::string> later;
    bool seen = false;
    for (const PresetGroup &item : file.groups) {
        if (item.id == groupId) {
            seen = true;
            continue;
        }
        if (!seen || item.options.empty()) {
            continue;
        }
        for (const std::string &key : optionKeys(item.options.front())) {
            later.push_back(key);
        }
    }
    std::vector<std::string> owned;
    for (const std::string &key : optionKeys(group->options.front())) {
        if (std::find(later.begin(), later.end(), key) == later.end()) {
            owned.push_back(key);
        }
    }
    return owned;
}

ViewerBands viewerBands(int windowHeight, bool historyVisible) {
    constexpr int kPageTop = 12;
    constexpr int kPageBottom = 12;
    constexpr int kTop = 40;
    constexpr int kBarGap = 10;
    constexpr int kRowGap = 8;
    constexpr int kChip = 44;
    constexpr int kStatus = 36;
    constexpr int kHistory = 104;
    ViewerBands bands;
    bands.barTop = kPageTop;
    bands.barHeight = kTop;
    bands.chipTop = bands.barTop + bands.barHeight + kBarGap;
    bands.chipHeight = kChip + kRowGap + kChip;
    bands.pictureTop = bands.chipTop + bands.chipHeight + kRowGap;
    const int history = historyVisible ? kHistory : 0;
    const int bottom = windowHeight - kPageBottom - history - kStatus;
    bands.pictureHeight = std::max(0, bottom - bands.pictureTop);
    return bands;
}

std::filesystem::path dataHome() {
    if (const char *overrideHome = std::getenv("VOXPRINT_HOME")) {
        const std::string text(overrideHome);
        if (!text.empty()) {
            return std::filesystem::path(text);
        }
    }
#ifdef _WIN32
    const char *local = std::getenv("LOCALAPPDATA");
    const std::filesystem::path base = (local != nullptr && local[0] != '\0')
        ? std::filesystem::path(local)
        : std::filesystem::path(std::getenv("USERPROFILE") != nullptr ? std::getenv("USERPROFILE") : ".") / "AppData" / "Local";
    return base / "VOXPrint AI Suite";
#else
    const char *xdg = std::getenv("XDG_DATA_HOME");
    std::filesystem::path base;
    if (xdg != nullptr && xdg[0] == '/') {
        base = xdg;
    } else {
        const char *home = std::getenv("HOME");
        base = std::filesystem::path(home != nullptr ? home : ".") / ".local" / "share";
    }
    return base / "voxprint-ai-suite";
#endif
}

std::filesystem::path suiteSettingsPath() { return dataHome() / "state" / "suite.json"; }

std::filesystem::path appSettingsPath(const std::string &appId) {
    return dataHome() / "state" / appId / "image-settings.json";
}

Json readDocument(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> guard(cache().mutex);
    const std::string key = cacheKey(path);
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::is_regular_file(status)) {
        return emptyObject();
    }
    const auto mtime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::filesystem::last_write_time(path).time_since_epoch())
                           .count();
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        const auto found = cache().documents.find(key);
        return found == cache().documents.end() ? emptyObject() : found->second;
    }
    const std::pair<std::int64_t, std::uintmax_t> token{static_cast<std::int64_t>(mtime), size};
    if (cache().tokens[key] == token && cache().documents.count(key) != 0) {
        return cache().documents[key];
    }
    Json parsed;
    std::string parseError;
    const std::string text = readFile(path);
    if (!parseJson(text, parsed, parseError) || parsed.kind != Json::Kind::Object) {
        const auto found = cache().documents.find(key);
        return found == cache().documents.end() ? emptyObject() : found->second;
    }
    remember(path, parsed, token.first, token.second);
    return parsed;
}

bool writeOwnKeys(const std::filesystem::path &path, const std::map<std::string, Json> &ownKeys, std::string &error) {
    for (const auto &item : ownKeys) {
        if (reservedSettingKey(item.first)) {
            error = "reserved key " + item.first;
            return false;
        }
    }
    FileLock lock(path);
    if (!lock.ok()) {
        error = "could not lock the file";
        return false;
    }
    Json current;
    {
        std::lock_guard<std::mutex> guard(cache().mutex);
        Json parsed;
        std::string parseError;
        const std::string text = readFile(path);
        if (!text.empty() && parseJson(text, parsed, parseError) && parsed.kind == Json::Kind::Object) {
            current = std::move(parsed);
        } else {
            const auto found = cache().documents.find(cacheKey(path));
            current = found == cache().documents.end() ? emptyObject() : found->second;
        }
    }
    if (current.kind != Json::Kind::Object) {
        current = emptyObject();
    }
    if (current.field("schema") == nullptr) {
        current.set("schema", Json::fromInt(1));
    }
    for (const auto &item : ownKeys) {
        current.set(item.first, item.second);
    }
    const std::string bytes = dumpJson(current) + "\n";
    if (!atomicReplace(path, bytes, error)) {
        return false;
    }
    std::error_code statError;
    const auto mtime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::filesystem::last_write_time(path).time_since_epoch())
                           .count();
    const auto size = std::filesystem::file_size(path, statError);
    std::lock_guard<std::mutex> guard(cache().mutex);
    if (!statError) {
        remember(path, current, static_cast<std::int64_t>(mtime), size);
    }
    return true;
}

bool readExpertMode() {
    const Json document = readDocument(suiteSettingsPath());
    const Json *flag = document.field("ui.expert_mode");
    return flag != nullptr && flag->kind == Json::Kind::Bool && flag->boolean;
}

bool writeExpertMode(bool enabled, std::string &error) {
    std::map<std::string, Json> own;
    own.emplace("ui.expert_mode", Json::fromBool(enabled));
    return writeOwnKeys(suiteSettingsPath(), own, error);
}

namespace {

int clampResizePercent(int percent) {
    return std::clamp(percent, 1, 1000);
}

int clampResizeSide(int side) {
    return std::clamp(side, 1, 100000);
}

int pairedSide(int edited, int editedSource, int otherSource) {
    if (editedSource < 1) {
        return 1;
    }
    const double value = static_cast<double>(edited) * static_cast<double>(otherSource) / static_cast<double>(editedSource);
    return clampResizeSide(static_cast<int>(std::lround(value)));
}

int percentOf(int side, int source) {
    if (source < 1) {
        return 100;
    }
    return clampResizePercent(static_cast<int>(std::lround(100.0 * static_cast<double>(side) / static_cast<double>(source))));
}

}  // namespace

ResizeSpec resizeFromSource(int sourceWidth, int sourceHeight, bool keepAspect, bool byPercent, bool smooth) {
    ResizeSpec spec;
    spec.sourceWidth = std::max(1, sourceWidth);
    spec.sourceHeight = std::max(1, sourceHeight);
    spec.width = spec.sourceWidth;
    spec.height = spec.sourceHeight;
    spec.percent = 100;
    spec.keepAspect = keepAspect;
    spec.byPercent = byPercent;
    spec.smooth = smooth;
    return spec;
}

ResizeSpec resizeFromWidth(ResizeSpec spec, int width) {
    spec.byPercent = false;
    spec.width = clampResizeSide(width);
    if (spec.keepAspect) {
        spec.height = pairedSide(spec.width, spec.sourceWidth, spec.sourceHeight);
    }
    spec.percent = percentOf(spec.width, spec.sourceWidth);
    return spec;
}

ResizeSpec resizeFromHeight(ResizeSpec spec, int height) {
    spec.byPercent = false;
    spec.height = clampResizeSide(height);
    if (spec.keepAspect) {
        spec.width = pairedSide(spec.height, spec.sourceHeight, spec.sourceWidth);
    }
    spec.percent = percentOf(spec.width, spec.sourceWidth);
    return spec;
}

ResizeSpec resizeFromPercent(ResizeSpec spec, int percent) {
    spec.byPercent = true;
    spec.percent = clampResizePercent(percent);
    spec.width = clampResizeSide(static_cast<int>(std::lround(spec.sourceWidth * (spec.percent / 100.0))));
    if (spec.keepAspect) {
        spec.height = pairedSide(spec.width, spec.sourceWidth, spec.sourceHeight);
    } else {
        spec.height = clampResizeSide(static_cast<int>(std::lround(spec.sourceHeight * (spec.percent / 100.0))));
    }
    return spec;
}

ResizeSpec resizeKeepAspect(ResizeSpec spec, bool keep) {
    spec.keepAspect = keep;
    if (!keep) {
        return spec;
    }
    if (spec.byPercent) {
        return resizeFromPercent(spec, spec.percent);
    }
    return resizeFromWidth(spec, spec.width);
}

}  // namespace lucent
