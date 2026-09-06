#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "keyboard/fixed_text_protocol.h"
#include "keyboard/keymap.h"

namespace ai_keyboard {

inline constexpr std::uint8_t kHostActionAppCommandKind = 0x05;
inline constexpr std::size_t kHostActionUuidAsciiLen = 36;

// Encodes one complete App Command payload for a canonical host action value.
// The wire fields are frozen: kind 0x05, chunk index 0, total chunks 1 and
// data length 36; bytes [4..39] carry the UUID ASCII without the
// "host_action:" prefix, and [40..62] stay zero as the existing container
// margin. Returns false and leaves out untouched when the value is not a
// canonical host action value, so callers fail closed. The report rides the
// existing Report ID 0x11 App Command container; kind 0x04 stays reserved
// for status responses.
inline bool encode_host_action_report(
    std::string_view value,
    std::array<std::uint8_t, kFixedTextAppCommandPayloadLen>& out) {
  if (!is_canonical_host_action_value(value)) {
    return false;
  }
  out.fill(0);
  out[0] = kHostActionAppCommandKind;
  out[1] = 0;
  out[2] = 1;
  out[3] = static_cast<std::uint8_t>(kHostActionUuidAsciiLen);
  const auto uuid = value.substr(value.size() - kHostActionUuidAsciiLen);
  for (std::size_t i = 0; i < kHostActionUuidAsciiLen; ++i) {
    out[kFixedTextAppCommandHeaderLen + i] =
        static_cast<std::uint8_t>(uuid[i]);
  }
  return true;
}

}  // namespace ai_keyboard
