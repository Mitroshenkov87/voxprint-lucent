// Copyright 2026 Aleksandr Mitroshenkov
// SPDX-License-Identifier: Apache-2.0

#include "shell_logic.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace {

int failures = 0;

void expect(bool ok, const char *message) {
    if (!ok) {
        std::cerr << message << '\n';
        ++failures;
    }
}

lucent::Json load(const std::filesystem::path &path) {
    lucent::Json json;
    std::string error;
    expect(lucent::parseJson(lucent::readFile(path), json, error), path.string().c_str());
    return json;
}

lucent::PresetFile presets(const std::filesystem::path &path) {
    lucent::PresetFile file;
    std::string error;
    expect(lucent::loadPresetDocument(load(path), file, error), error.c_str());
    return file;
}

std::map<std::string, lucent::Json> defaultsFrom(const lucent::Json &object) {
    std::map<std::string, lucent::Json> values;
    for (const auto &item : object.object) {
        values.emplace(item.first, item.second);
    }
    return values;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "usage: shell_logic_test FIXTURE_DIR\n";
        return 2;
    }
    const std::filesystem::path dir(argv[1]);
    const auto defaults = defaultsFrom(load(dir / "defaults.json"));
    const lucent::PresetFile quality = presets(dir / "quality.json");
    lucent::SettingState merged = lucent::mergePresets(quality, defaults, nullptr);
    expect(merged.values["shell.steps"].integer == 20, "balanced steps win over the default");
    expect(merged.values["shell.guidance"].asNumber() == 7.0, "balanced guidance is written");
    expect(merged.values["shell.sampler"].text == "euler", "balanced sampler is written");
    expect(merged.values["shell.note"].text == "kept", "a key the preset does not write stays");
    const lucent::ChipStatus exact = lucent::chipFor(quality, merged, "quality");
    expect(exact.kind == lucent::ChipKind::Exact && exact.optionId == "balanced", "an exact match names the option");
    expect(lucent::formatChip(exact.kind, "Balanced", "{name} + changes", "Custom") == "Balanced", "an exact chip is the name");

    const lucent::Json close = load(dir / "stored-close.json");
    lucent::SettingState changed = lucent::mergePresets(quality, defaults, &close);
    const lucent::ChipStatus modified = lucent::chipFor(quality, changed, "quality");
    expect(modified.kind == lucent::ChipKind::Modified, "one of three keys is close");
    expect(changed.overrides["quality"].size() == 1 && changed.overrides["quality"][0] == "shell.steps",
           "the differing key is the override");
    expect(
        lucent::formatChip(modified.kind, "Balanced", "{name} + changes", "Custom") == "Balanced + changes",
        "a close edit reads name + changes");
    expect(lucent::resetGroup(quality, changed, "quality"), "reset restores the stored option");
    expect(changed.values["shell.steps"].integer == 20, "reset writes the option steps");
    expect(changed.values["shell.guidance"].asNumber() == 7.0, "reset writes the option guidance");
    expect(changed.values["shell.sampler"].text == "euler", "reset writes the option sampler");
    expect(changed.values["shell.note"].text == "kept", "reset leaves keys the option does not write");

    const lucent::Json far = load(dir / "stored-far.json");
    lucent::SettingState custom = lucent::mergePresets(quality, defaults, &far);
    const lucent::ChipStatus customChip = lucent::chipFor(quality, custom, "quality");
    expect(customChip.kind == lucent::ChipKind::Custom && customChip.optionId == "balanced",
           "two of three keys read Custom and keep the stored choice");
    expect(lucent::formatChip(customChip.kind, "Balanced", "{name} + changes", "Custom") == "Custom", "a far edit reads Custom");
    expect(lucent::resetGroup(quality, custom, "quality"), "reset still restores a custom choice");
    expect(custom.values["shell.steps"].integer == 20 && custom.values["shell.guidance"].asNumber() == 7.0, "custom reset writes the stored option");

    const lucent::PresetFile one = presets(dir / "one-key.json");
    lucent::SettingState zoom = lucent::mergePresets(one, {}, nullptr);
    expect(lucent::editValue(one, zoom, "viewer.zoom", lucent::Json::fromString("free")), "a hand edit is stored");
    const lucent::ChipStatus single = lucent::chipFor(one, zoom, "zoom");
    expect(single.kind == lucent::ChipKind::Custom, "a one-key group has no close edit");

    const lucent::PresetFile overlap = presets(dir / "overlap.json");
    lucent::SettingState layered = lucent::mergePresets(overlap, defaults, nullptr);
    expect(layered.values["shell.steps"].integer == 30, "the later group owns the shared key");
    expect(lucent::chipFor(overlap, layered, "quality").kind == lucent::ChipKind::Exact, "the earlier group ignores the key it no longer owns");
    expect(lucent::ownedKeys(overlap, "quality").size() == 2, "quality keeps guidance and sampler");

    const lucent::PresetFile unavailable = presets(dir / "unavailable.json");
    lucent::SettingState output = lucent::mergePresets(unavailable, {}, nullptr);
    const lucent::Json before = output.values["shell.output"];
    expect(!lucent::chooseOption(unavailable, output, "output", "4k"), "an unavailable chip writes nothing");
    expect(lucent::sameValue(output.values["shell.output"], before), "the live value stays");

    lucent::PresetFile rejected;
    std::string safetyError;
    expect(!lucent::loadPresetDocument(load(dir / "safety.json"), rejected, safetyError), "a reserved key is rejected");
    expect(lucent::reservedSettingKey("safety"), "safety is reserved");
    expect(lucent::reservedSettingKey("safety.gate"), "safety.* is reserved");
    expect(lucent::reservedSettingKey("image.minor"), "a final minor segment is reserved");
    expect(lucent::reservedSettingKey("image.minors"), "a final minors segment is reserved");
    expect(lucent::reservedSettingKey("crop.age"), "a final age segment is reserved");
    expect(lucent::reservedSettingKey("viewer.guard"), "a final guard segment is reserved");
    expect(!lucent::reservedSettingKey("image.adult_content"), "adult content is allowed");
    lucent::PresetFile adult;
    std::string adultError;
    expect(lucent::loadPresetDocument(load(dir / "adult-ok.json"), adult, adultError), "adult content parses");

    const std::filesystem::path home = dir / "home";
    std::filesystem::remove_all(home);
    ::setenv("VOXPRINT_HOME", home.string().c_str(), 1);
    expect(lucent::dataHome() == home, "VOXPRINT_HOME replaces the suite root");
    const lucent::Json foreign = load(dir / "suite-foreign.json");
    std::string writeError;
    expect(lucent::writeOwnKeys(lucent::suiteSettingsPath(), {}, writeError), "the first publish creates the file");
    std::map<std::string, lucent::Json> seeded;
    for (const auto &item : foreign.object) {
        seeded.emplace(item.first, item.second);
    }
    expect(lucent::writeOwnKeys(lucent::suiteSettingsPath(), seeded, writeError), "foreign keys can be stored");
    expect(lucent::writeExpertMode(true, writeError), "expert mode is its own key");
    const lucent::Json suite = lucent::readDocument(lucent::suiteSettingsPath());
    expect(suite.field("ui.expert_mode") != nullptr && suite.field("ui.expert_mode")->boolean, "expert mode is stored");
    expect(suite.field("ui_language") != nullptr && suite.field("ui_language")->text == "de", "another app's language stays");
    expect(suite.field("gpu") != nullptr && suite.field("gpu")->text == "auto", "another app's gpu stays");
    expect(suite.field("keymap") != nullptr && suite.field("keymap")->kind == lucent::Json::Kind::Object, "another app's keymap stays");
    expect(std::filesystem::is_regular_file(std::filesystem::path(lucent::suiteSettingsPath().string() + ".lock")),
           "the lock file stays");
    {
        std::ofstream broken(lucent::suiteSettingsPath(), std::ios::binary | std::ios::trunc);
        broken << "{";
    }
    const lucent::Json kept = lucent::readDocument(lucent::suiteSettingsPath());
    expect(kept.field("ui_language") != nullptr && kept.field("ui_language")->text == "de", "an invalid file keeps the last good state");
    expect(lucent::writeExpertMode(false, writeError), "a writer repairs from the last good state");
    const lucent::Json repaired = lucent::readDocument(lucent::suiteSettingsPath());
    expect(repaired.field("ui.expert_mode") != nullptr && !repaired.field("ui.expert_mode")->boolean, "the repaired flag is false");
    expect(repaired.field("gpu") != nullptr && repaired.field("gpu")->text == "auto", "the repair keeps keys it does not own");
    std::filesystem::remove_all(home);

    if (failures != 0) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    return 0;
}
