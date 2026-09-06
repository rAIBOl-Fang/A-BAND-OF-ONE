#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>

#include "keyboard/config_payload.h"
#include "keyboard/fixed_text_protocol.h"
#include "keyboard/host_action_protocol.h"
#include "keyboard/keymap.h"
#include "keyboard/status_hid_protocol.h"

using ai_keyboard::ActionKind;
using ai_keyboard::ConfigParseStatus;
using ai_keyboard::FirmwareEventKind;
using ai_keyboard::InputId;
using ai_keyboard::InputPhase;

namespace {

// Fixed sample UUIDs for host tests only; never a real application mapping.
constexpr const char* kCanonicalUuid =
    "0f0e0d0c-0b0a-0908-0706-050403020100";
constexpr const char* kNilUuid =
    "00000000-0000-0000-0000-000000000000";
constexpr const char* kAnyVersionUuid =
    "a1b2c3d4-e5f6-4789-9876-543210fedcba";

// Per-key sample UUIDs proving each of KEY1-KEY8 keeps its own distinct value.
constexpr const char* const kEightKeyDistinctUuids[8] = {
    "10000000-0000-0000-0000-000000000001",
    "20000000-0000-0000-0000-000000000002",
    "30000000-0000-0000-0000-000000000003",
    "40000000-0000-0000-0000-000000000004",
    "50000000-0000-0000-0000-000000000005",
    "60000000-0000-0000-0000-000000000006",
    "70000000-0000-0000-0000-000000000007",
    "80000000-0000-0000-0000-000000000008",
};

constexpr const char* const kEightKeyNames[8] = {
    "KEY1", "KEY2", "KEY3", "KEY4", "KEY5", "KEY6", "KEY7", "KEY8",
};

constexpr InputId kEightKeyIds[8] = {
    InputId::Key1, InputId::Key2, InputId::Key3, InputId::Key4,
    InputId::Key5, InputId::Key6, InputId::Key7, InputId::Key8,
};

std::string host_action_value(const char* uuid) {
  return std::string("host_action:") + uuid;
}

std::string config_with_key_press(const char* key, const std::string& press) {
  std::string json =
      R"({"schema":"ai_keyboard.v1","target_platform":"macos",)"
      R"("profiles":[{"id":"default","keys":{)";
  for (std::size_t i = 0; i < 8; ++i) {
    if (i > 0) {
      json += ",";
    }
    const std::string name = kEightKeyNames[i];
    json += "\"" + name + "\":{\"press\":\"" +
            (name == key ? press : std::string("copy")) + "\"}";
  }
  json +=
      "},\"encoder\":{\"left\":\"disabled\",\"right\":\"disabled\",\"press\":\"disabled\"}}]}";
  return json;
}

std::string read_source_file(const char* relative_path) {
  std::ifstream input(
      std::string(EASY_INPUT_REPO_ROOT) + "/" + relative_path,
      std::ios::binary);
  assert(input.is_open());
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

}  // namespace

void canonical_lowercase_host_action_values_are_accepted() {
  assert(ai_keyboard::is_canonical_host_action_value(
      host_action_value(kCanonicalUuid)));
  // No UUID version restriction: any version nibble stays acceptable.
  assert(ai_keyboard::is_canonical_host_action_value(
      host_action_value(kAnyVersionUuid)));
  // No nil-UUID restriction: the all-zero UUID stays acceptable.
  assert(ai_keyboard::is_canonical_host_action_value(
      host_action_value(kNilUuid)));
}

void non_canonical_host_action_values_are_rejected() {
  // Uppercase is rejected, not auto-lowercased (full and partial).
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0F0E0D0C-0B0A-0908-0706-050403020100"));
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0c-0B0a-0908-0706-050403020100"));
  // Wrong length (35 and 37 characters after the prefix).
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0c-0b0a-0908-0706-05040302010"));
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0c-0b0a-0908-0706-0504030201000"));
  // Hyphen at a wrong position (same total length).
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0c0-b0a-0908-0706-050403020100"));
  // Extra hyphen inside a data group (same total length).
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0c-0b0a-0908-0706-0504030-0100"));
  // Non-hex characters.
  assert(!ai_keyboard::is_canonical_host_action_value(
      "host_action:0f0e0d0g-0b0a-0908-0706-050403020100"));
  // Missing prefix, empty value, empty input.
  assert(!ai_keyboard::is_canonical_host_action_value(kCanonicalUuid));
  assert(!ai_keyboard::is_canonical_host_action_value("host_action:"));
  assert(!ai_keyboard::is_canonical_host_action_value(""));
}

void config_payload_preserves_full_host_action_string() {
  const auto result = ai_keyboard::parse_config_payload(
      config_with_key_press("KEY1", host_action_value(kCanonicalUuid)));
  assert(result.status == ConfigParseStatus::Ok);
  const auto& action = result.config.keymap.action_for(InputId::Key1);
  assert(action.kind == ActionKind::HostAction);
  assert(action.text == host_action_value(kCanonicalUuid));
  assert(action.hotkey.empty());
}

void config_payload_fails_closed_on_malformed_host_action_values() {
  const std::string malformed[] = {
      "host_action:0F0E0D0C-0B0A-0908-0706-050403020100",
      "host_action:0f0e0d0c-0b0a-0908-0706-05040302010",
      "host_action:0f0e0d0c0-b0a-0908-0706-050403020100",
      "host_action:0f0e0d0g-0b0a-0908-0706-050403020100",
  };
  for (const auto& press : malformed) {
    const auto result =
        ai_keyboard::parse_config_payload(config_with_key_press("KEY1", press));
    assert(result.status == ConfigParseStatus::UnknownAction);
  }
}

void host_action_press_emits_one_app_command_event_release_none() {
  const ai_keyboard::Action action(
      ActionKind::HostAction, "", host_action_value(kCanonicalUuid));
  const auto press = ai_keyboard::event_for_action(
      action, InputPhase::Pressed, "RightMeta", "RightOption");
  assert(press.kind == FirmwareEventKind::AppCommand);
  assert(press.value == host_action_value(kCanonicalUuid));

  const auto release = ai_keyboard::event_for_action(
      action, InputPhase::Released, "RightMeta", "RightOption");
  assert(release.kind == FirmwareEventKind::None);
}

void host_action_report_encoding_uses_frozen_wire_fields() {
  std::array<std::uint8_t, ai_keyboard::kFixedTextAppCommandPayloadLen> report{};
  report.fill(0xFF);
  assert(ai_keyboard::encode_host_action_report(
      host_action_value(kCanonicalUuid), report));

  assert(report[0] == 0x05);
  assert(report[1] == 0);
  assert(report[2] == 1);
  assert(report[3] == 36);
  for (std::size_t i = 0; i < 36; ++i) {
    assert(report[4 + i] == static_cast<std::uint8_t>(kCanonicalUuid[i]));
  }
  for (std::size_t i = 40; i < report.size(); ++i) {
    assert(report[i] == 0);
  }

  // The report rides the existing 0x11 App Command container, and kind 0x04
  // stays reserved for status responses.
  assert(ai_keyboard::kFixedTextAppCommandReportId == 0x11);
  assert(ai_keyboard::kStatusResponseCommandKind == 0x04);
  assert(ai_keyboard::kHostActionAppCommandKind == 0x05);
  assert(ai_keyboard::kHostActionAppCommandKind !=
         ai_keyboard::kStatusResponseCommandKind);
}

void host_action_report_encoding_rejects_non_canonical_values() {
  std::array<std::uint8_t, ai_keyboard::kFixedTextAppCommandPayloadLen> report{};
  report.fill(0xEE);
  const std::string rejected[] = {
      "host_action:0F0E0D0C-0B0A-0908-0706-050403020100",
      "host_action:0f0e0d0c-0b0a-0908-0706-05040302010",
      "host_action:0f0e0d0c0-b0a-0908-0706-050403020100",
      "host_action:0f0e0d0g-0b0a-0908-0706-050403020100",
      "copy",
      "",
  };
  for (const auto& value : rejected) {
    assert(!ai_keyboard::encode_host_action_report(value, report));
  }
  assert(report[0] == 0xEE);
}

void eight_keys_each_parse_and_preserve_their_own_host_action() {
  for (std::size_t i = 0; i < 8; ++i) {
    const std::string value =
        host_action_value(kEightKeyDistinctUuids[i]);
    const auto result = ai_keyboard::parse_config_payload(
        config_with_key_press(kEightKeyNames[i], value));
    assert(result.status == ConfigParseStatus::Ok);

    const auto& action = result.config.keymap.action_for(kEightKeyIds[i]);
    assert(action.kind == ActionKind::HostAction);
    assert(action.text == value);
    assert(action.hotkey.empty());

    // The other seven keys still hold their shared-chain legacy action.
    for (std::size_t j = 0; j < 8; ++j) {
      if (j == i) {
        continue;
      }
      const auto& other = result.config.keymap.action_for(kEightKeyIds[j]);
      assert(other.kind == ActionKind::Copy);
      assert(other.text.empty());
    }
  }
}

void eight_keys_each_emit_one_host_action_press_and_no_release() {
  for (std::size_t i = 0; i < 8; ++i) {
    const std::string value =
        host_action_value(kEightKeyDistinctUuids[i]);
    const auto result = ai_keyboard::parse_config_payload(
        config_with_key_press(kEightKeyNames[i], value));
    assert(result.status == ConfigParseStatus::Ok);

    const auto& action = result.config.keymap.action_for(kEightKeyIds[i]);
    const auto press = ai_keyboard::event_for_action(
        action, InputPhase::Pressed, "RightMeta", "RightOption");
    assert(press.kind == FirmwareEventKind::AppCommand);
    assert(press.value == value);

    const auto release = ai_keyboard::event_for_action(
        action, InputPhase::Released, "RightMeta", "RightOption");
    assert(release.kind == FirmwareEventKind::None);
  }
}

void legacy_actions_regress_alongside_host_action_in_one_payload() {
  const std::string payload =
      R"({"schema":"ai_keyboard.v1","target_platform":"macos",)"
      R"("profiles":[{"id":"default","keys":{)"
      R"("KEY1":{"press":")" +
      host_action_value(kCanonicalUuid) +
      R"("},)"
      R"("KEY2":{"press":"copy"},)"
      R"("KEY3":{"press":"paste"},)"
      R"("KEY4":{"press":{"hotkey":"Meta+Shift+K"}},)"
      R"("KEY5":{"press":{"text":"回归"}},)"
      R"("KEY6":{"press":"undo"},)"
      R"("KEY7":{"press":"select_all"},)"
      R"("KEY8":{"press":"disabled"}},)"
      R"("encoder":{"left":"disabled","right":"disabled","press":"disabled"}}]})";
  const auto result = ai_keyboard::parse_config_payload(payload);
  assert(result.status == ConfigParseStatus::Ok);
  const auto& keymap = result.config.keymap;

  const auto press = [keymap](InputId input) {
    return ai_keyboard::event_for_action(keymap.action_for(input),
                                          InputPhase::Pressed,
                                          "RightMeta",
                                          "RightOption");
  };
  const auto release = [keymap](InputId input) {
    return ai_keyboard::event_for_action(keymap.action_for(input),
                                          InputPhase::Released,
                                          "RightMeta",
                                          "RightOption");
  };

  // New capability alongside legacy actions in one payload.
  assert(press(InputId::Key1).kind == FirmwareEventKind::AppCommand);
  assert(press(InputId::Key1).value == host_action_value(kCanonicalUuid));
  assert(release(InputId::Key1).kind == FirmwareEventKind::None);

  // Copy: semantic macOS hotkey on press and release.
  assert(press(InputId::Key2).kind == FirmwareEventKind::HidKeyDown);
  assert(press(InputId::Key2).value == "Meta+C");
  assert(release(InputId::Key2).kind == FirmwareEventKind::HidKeyUp);
  assert(release(InputId::Key2).value == "Meta+C");

  // Paste.
  assert(press(InputId::Key3).kind == FirmwareEventKind::HidKeyDown);
  assert(press(InputId::Key3).value == "Meta+V");
  assert(release(InputId::Key3).kind == FirmwareEventKind::HidKeyUp);
  assert(release(InputId::Key3).value == "Meta+V");

  // Custom hotkey object.
  assert(press(InputId::Key4).kind == FirmwareEventKind::HidKeyDown);
  assert(press(InputId::Key4).value == "Meta+Shift+K");
  assert(release(InputId::Key4).kind == FirmwareEventKind::HidKeyUp);
  assert(release(InputId::Key4).value == "Meta+Shift+K");

  // Fixed text: one event on press, none on release.
  assert(press(InputId::Key5).kind == FirmwareEventKind::FixedText);
  assert(press(InputId::Key5).value == "回归");
  assert(release(InputId::Key5).kind == FirmwareEventKind::None);

  // Undo and select_all keep their semantic hotkeys.
  assert(press(InputId::Key6).value == "Meta+Z");
  assert(release(InputId::Key6).value == "Meta+Z");
  assert(press(InputId::Key7).value == "Meta+A");
  assert(release(InputId::Key7).value == "Meta+A");

  // Disabled stays silent.
  assert(press(InputId::Key8).kind == FirmwareEventKind::None);
  assert(release(InputId::Key8).kind == FirmwareEventKind::None);
}

void default_keymap_and_production_sources_contain_no_sample_host_action() {
  // The factory keymap never ships a HostAction binding or host_action text.
  const auto keymap = ai_keyboard::DefaultKeymap();
  const auto input_count = static_cast<std::size_t>(InputId::Count);
  for (std::size_t i = 0; i < input_count; ++i) {
    const auto& action = keymap.action_for(static_cast<InputId>(i));
    assert(action.kind != ActionKind::HostAction);
    assert(action.text.find("host_action:") == std::string::npos);
  }

  // Sample UUIDs live only in host tests, never in production sources.
  const char* const sources[] = {
      "components/keyboard/src/keymap.cpp",
      "components/keyboard/src/config_payload.cpp",
      "main/app_main.cpp",
  };
  const char* const forbidden[] = {
      kCanonicalUuid,
      kAnyVersionUuid,
      kNilUuid,
      kEightKeyDistinctUuids[0],
      kEightKeyDistinctUuids[7],
  };
  for (const auto* path : sources) {
    const auto source = read_source_file(path);
    for (const auto* uuid : forbidden) {
      assert(source.find(uuid) == std::string::npos);
    }
  }
}

int main() {
  canonical_lowercase_host_action_values_are_accepted();
  non_canonical_host_action_values_are_rejected();
  config_payload_preserves_full_host_action_string();
  config_payload_fails_closed_on_malformed_host_action_values();
  host_action_press_emits_one_app_command_event_release_none();
  host_action_report_encoding_uses_frozen_wire_fields();
  host_action_report_encoding_rejects_non_canonical_values();
  eight_keys_each_parse_and_preserve_their_own_host_action();
  eight_keys_each_emit_one_host_action_press_and_no_release();
  legacy_actions_regress_alongside_host_action_in_one_payload();
  default_keymap_and_production_sources_contain_no_sample_host_action();
  return 0;
}
