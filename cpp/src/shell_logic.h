// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace lucent {

/// One JSON value. Objects keep file order.
struct Json {
    enum class Kind { Null, Bool, Int, Float, String, Array, Object };

    Kind kind = Kind::Null;
    bool boolean = false;
    long long integer = 0;
    double number = 0;
    std::string text;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    /// A null value.
    static Json nul();
    /// A boolean.
    static Json fromBool(bool value);
    /// An integer.
    static Json fromInt(long long value);
    /// A non-integer number.
    static Json fromFloat(double value);
    /// A string.
    static Json fromString(std::string value);

    /// The field named `key`, or null when it is absent.
    const Json *field(const std::string &key) const;
    /// Insert or replace `key`.
    void set(const std::string &key, Json value);
    /// True when this value is an integer or a float.
    bool isNumber() const;
    /// The numeric value of an integer or a float.
    double asNumber() const;
};

/// Parse `text`. `error` explains a refusal.
bool parseJson(const std::string &text, Json &out, std::string &error);

/// Serialize `value` as UTF-8 JSON.
std::string dumpJson(const Json &value);

/// Read a whole file. Empty when it cannot be read.
std::string readFile(const std::filesystem::path &path);

/// True when two preset values are the same number, text, or boolean.
bool sameValue(const Json &left, const Json &right);

/// True for `safety`, a `safety.` key, or a final segment minor, minors, age, or guard.
bool reservedSettingKey(const std::string &key);

/// One chip and the values it writes, in file order.
struct PresetOption {
    std::string id;
    std::string labelKey;
    std::vector<std::pair<std::string, Json>> values;
    bool available = true;
    std::string reasonKey;
    std::string actionLabelKey;
    std::string actionId;
};

/// One row of chips. Every option writes the same keys.
struct PresetGroup {
    std::string id;
    std::string labelKey;
    std::string defaultId;
    std::vector<PresetOption> options;
};

/// A validated `voxprint-presets/1` document.
struct PresetFile {
    std::vector<PresetGroup> groups;
};

/// How the live values relate to the stored option.
enum class ChipKind { Exact, Modified, Custom };

/// The chip label for one group.
struct ChipStatus {
    ChipKind kind = ChipKind::Exact;
    std::string optionId;
    std::string labelKey;
};

/// Live values, the chosen option, and the keys that differ.
struct SettingState {
    std::map<std::string, Json> values;
    std::map<std::string, std::string> chosen;
    std::map<std::string, std::vector<std::string>> overrides;
};

/// Validate a preset document.
///
/// `error` names the first problem. A reserved key is a problem.
bool loadPresetDocument(const Json &document, PresetFile &out, std::string &error);

/// Find a group, or null.
const PresetGroup *findGroup(const PresetFile &file, const std::string &groupId);

/// Find an option, or null.
const PresetOption *findOption(const PresetGroup &group, const std::string &optionId);

/// Mark one option available or grey. Reason fields already on the option stay.
bool setOptionAvailable(PresetFile &file, const std::string &groupId, const std::string &optionId, bool available);

/// Build state from defaults, then groups, then a stored schema-1 document.
///
/// A null `stored` document, or one that is not schema 1, is skipped.
/// A stored reserved key rejects that document and leaves the merged presets.
SettingState mergePresets(const PresetFile &file, const std::map<std::string, Json> &defaults, const Json *stored);

/// Classify one group. Unknown groups come back as custom with an empty id.
ChipStatus chipFor(const PresetFile &file, const SettingState &state, const std::string &groupId);

/// "{name} + changes", the option name, or the custom word.
std::string formatChip(ChipKind kind, const std::string &name, const std::string &changedPattern, const std::string &customWord);

/// Write one option. An unavailable or unknown option writes nothing.
bool chooseOption(const PresetFile &file, SettingState &state, const std::string &groupId, const std::string &optionId);

/// Hand-edit one key. A reserved key is refused.
bool editValue(const PresetFile &file, SettingState &state, const std::string &key, const Json &value);

/// Restore the stored option. Keys that option does not write stay.
bool resetGroup(const PresetFile &file, SettingState &state, const std::string &groupId);

/// Keys this group owns. A later group that writes the same key owns it.
std::vector<std::string> ownedKeys(const PresetFile &file, const std::string &groupId);

/// The suite data root.
///
/// `VOXPRINT_HOME`, when set and not empty, replaces the platform directory.
/// This is the only function that spells that directory.
std::filesystem::path dataHome();

/// `<data home>/state/suite.json`.
std::filesystem::path suiteSettingsPath();

/// `<data home>/state/<app-id>/image-settings.json`.
std::filesystem::path appSettingsPath(const std::string &appId);

/// Read a JSON object, keeping the last good object when the file is invalid.
///
/// A missing file is an empty object. The cache is per path in this process.
Json readDocument(const std::filesystem::path &path);

/// Lock `<path>.lock`, re-read, replace only `ownKeys`, and publish atomically.
///
/// The lock file stays after release. An invalid current file is merged onto
/// the last good object. A reserved key in `ownKeys` is refused.
bool writeOwnKeys(const std::filesystem::path &path, const std::map<std::string, Json> &ownKeys, std::string &error);

/// True when Expert mode is stored. Missing or invalid storage is false.
bool readExpertMode();

/// Store Expert mode without touching any other suite key.
bool writeExpertMode(bool enabled, std::string &error);

}  // namespace lucent
