#include "abo_host/wire_messages.h"

#include <cstdarg>
#include <cstdio>

namespace abo_host {
namespace {

constexpr std::size_t kMaxKeys = 8;

bool append_text(char* output,
                 std::size_t capacity,
                 std::size_t* used,
                 std::string_view text) {
  if (output == nullptr || used == nullptr || *used > capacity ||
      text.size() > capacity - *used) {
    return false;
  }
  for (std::size_t i = 0; i < text.size(); ++i) {
    output[*used + i] = text[i];
  }
  *used += text.size();
  return true;
}

bool append_format(char* output,
                   std::size_t capacity,
                   std::size_t* used,
                   const char* format,
                   ...) {
  if (output == nullptr || used == nullptr || format == nullptr ||
      *used >= capacity) {
    return false;
  }
  va_list arguments;
  va_start(arguments, format);
  const int written = std::vsnprintf(output + *used, capacity - *used, format,
                                     arguments);
  va_end(arguments);
  if (written < 0 || static_cast<std::size_t>(written) >= capacity - *used) {
    return false;
  }
  *used += static_cast<std::size_t>(written);
  return true;
}

bool append_json_string(char* output,
                        std::size_t capacity,
                        std::size_t* used,
                        std::string_view value) {
  if (!append_text(output, capacity, used, "\"")) return false;
  for (const unsigned char byte : value) {
    switch (byte) {
      case '"':
        if (!append_text(output, capacity, used, "\\\"")) return false;
        break;
      case '\\':
        if (!append_text(output, capacity, used, "\\\\")) return false;
        break;
      case '\b':
        if (!append_text(output, capacity, used, "\\b")) return false;
        break;
      case '\f':
        if (!append_text(output, capacity, used, "\\f")) return false;
        break;
      case '\n':
        if (!append_text(output, capacity, used, "\\n")) return false;
        break;
      case '\r':
        if (!append_text(output, capacity, used, "\\r")) return false;
        break;
      case '\t':
        if (!append_text(output, capacity, used, "\\t")) return false;
        break;
      default:
        if (byte < 0x20U) {
          if (!append_format(output, capacity, used, "\\u%04x", byte)) {
            return false;
          }
        } else if (!append_text(
                       output, capacity, used,
                       std::string_view(reinterpret_cast<const char*>(&byte),
                                        1))) {
          return false;
        }
        break;
    }
  }
  return append_text(output, capacity, used, "\"");
}

std::size_t finish(char* output, std::size_t capacity, std::size_t used) {
  if (output == nullptr || used >= capacity) return 0;
  output[used] = '\0';
  return used;
}

std::size_t format_ack_impl(char* output,
                            std::size_t capacity,
                            std::string_view command,
                            std::string_view tx,
                            std::string_view score_id,
                            std::uint8_t score_version,
                            bool include_score) {
  std::size_t used = 0;
  if (!append_text(output, capacity, &used,
                   "{\"t\":\"ack\",\"v\":1,\"cmd\":")) {
    return 0;
  }
  if (!append_json_string(output, capacity, &used, command) ||
      !append_text(output, capacity, &used, ",\"tx\":") ||
      !append_json_string(output, capacity, &used, tx)) {
    return 0;
  }
  if (include_score) {
    if (!append_text(output, capacity, &used, ",\"score_id\":") ||
        !append_json_string(output, capacity, &used, score_id) ||
        !append_format(output, capacity, &used, ",\"score_version\":%u",
                        static_cast<unsigned>(score_version))) {
      return 0;
    }
  }
  if (!append_text(output, capacity, &used, "}\n")) return 0;
  return finish(output, capacity, used);
}

}  // namespace

std::size_t format_ack(char* output,
                       std::size_t capacity,
                       std::string_view command,
                       std::string_view tx) {
  return format_ack_impl(output, capacity, command, tx, {}, 0, false);
}

std::size_t format_score_commit_ack(char* output,
                                    std::size_t capacity,
                                    std::string_view tx,
                                    std::string_view score_id,
                                    std::uint8_t score_version) {
  return format_ack_impl(output, capacity, "score_commit", tx, score_id,
                         score_version, true);
}

std::size_t format_judge(char* output,
                         std::size_t capacity,
                         bool ok,
                         std::uint8_t target,
                         std::uint8_t expected,
                         std::uint16_t errors) {
  std::size_t used = 0;
  if (!append_format(output, capacity, &used,
                     "{\"t\":\"judge\",\"v\":1,\"ok\":%s,"
                     "\"target\":%u,\"expected\":%u,\"errors\":%u}\n",
                     ok ? "true" : "false", static_cast<unsigned>(target),
                     static_cast<unsigned>(expected),
                     static_cast<unsigned>(errors))) {
    return 0;
  }
  return finish(output, capacity, used);
}

std::size_t format_result(char* output,
                          std::size_t capacity,
                          std::uint16_t errors,
                          std::uint32_t elapsed_ms) {
  std::size_t used = 0;
  if (!append_format(output, capacity, &used,
                     "{\"t\":\"result\",\"v\":1,\"errors\":%u,"
                     "\"elapsed\":%lu,\"elapsed_ms\":%lu}\n",
                     static_cast<unsigned>(errors),
                     static_cast<unsigned long>(elapsed_ms),
                     static_cast<unsigned long>(elapsed_ms))) {
    return 0;
  }
  return finish(output, capacity, used);
}

std::size_t format_input(char* output,
                         std::size_t capacity,
                         std::uint32_t sequence,
                         std::string_view control,
                         std::string_view phase) {
  if (control != "s1" && control != "s2" && control != "s3" &&
      control != "s4" && control != "s5" && control != "s6" &&
      control != "s7" && control != "s8" && control != "s9") {
    return 0;
  }
  if (phase != "pressed" && phase != "released") return 0;

  std::size_t used = 0;
  if (!append_format(output, capacity, &used,
                     "{\"t\":\"input\",\"v\":1,\"seq\":%lu,"
                     "\"control\":",
                     static_cast<unsigned long>(sequence)) ||
      !append_json_string(output, capacity, &used, control) ||
      !append_text(output, capacity, &used, ",\"phase\":") ||
      !append_json_string(output, capacity, &used, phase) ||
      !append_text(output, capacity, &used, "}\n")) {
    return 0;
  }
  return finish(output, capacity, used);
}

std::size_t format_state(char* output,
                         std::size_t capacity,
                         const WireStateView& state) {
  if (state.mode == nullptr || state.keys == nullptr ||
      state.key_count > kMaxKeys || state.note_ticks > state.note_total_ticks) {
    return 0;
  }
  std::size_t used = 0;
  if (!append_text(output, capacity, &used,
                   "{\"t\":\"state\",\"v\":1,\"mode\":") ||
      !append_json_string(output, capacity, &used, state.mode) ||
      !append_text(output, capacity, &used, ",\"phase\":") ||
      !append_json_string(output, capacity, &used,
                          state.phase == nullptr ? "standby" : state.phase) ||
      !append_text(output, capacity, &used, ",\"subphase\":") ||
      !append_json_string(output, capacity, &used,
                          state.subphase == nullptr ? "none" : state.subphase) ||
      !append_format(output, capacity, &used,
                     ",\"instrument\":%u,\"instrument_loading\":%s,"
                     "\"octave\":%u,\"bpm\":%u,\"errors\":%u,"
                     "\"cursor\":%u,\"elapsed\":%lu,\"elapsed_ms\":%lu,"
                     "\"score_crc32\":\"%08lx\",\"note_ticks\":%lu,"
                     "\"note_total_ticks\":%lu,"
                     "\"knob\":%d,\"volume\":%u,"
                     "\"leds\":[%s,%s,%s,%s,%s],\"keys\":[",
                     static_cast<unsigned>(state.instrument),
                     state.instrument_loading ? "true" : "false",
                     static_cast<unsigned>(state.octave),
                     static_cast<unsigned>(state.bpm),
                     static_cast<unsigned>(state.errors),
                     static_cast<unsigned>(state.cursor),
                     static_cast<unsigned long>(state.elapsed),
                     static_cast<unsigned long>(state.elapsed_ms),
                     static_cast<unsigned long>(state.score_crc32),
                     static_cast<unsigned long>(state.note_ticks),
                     static_cast<unsigned long>(state.note_total_ticks),
                     static_cast<int>(state.knob),
                     static_cast<unsigned>(state.volume),
                     state.leds[0] ? "true" : "false",
                     state.leds[1] ? "true" : "false",
                     state.leds[2] ? "true" : "false",
                     state.leds[3] ? "true" : "false",
                     state.leds[4] ? "true" : "false")) {
    return 0;
  }
  for (std::size_t i = 0; i < state.key_count; ++i) {
    if (i > 0 && !append_text(output, capacity, &used, ",")) return 0;
    if (state.keys[i] == nullptr ||
        !append_json_string(output, capacity, &used, state.keys[i])) {
      return 0;
    }
  }
  if (!append_text(output, capacity, &used, "],\"score_loaded\":") ||
      !append_text(output, capacity, &used,
                   state.score_loaded ? "true" : "false") ||
      !append_text(output, capacity, &used, ",\"score_id\":") ||
      !append_json_string(output, capacity, &used, state.score_id) ||
      !append_format(output, capacity, &used, ",\"score_version\":%u}\n",
                     static_cast<unsigned>(state.score_version))) {
    return 0;
  }
  return finish(output, capacity, used);
}

}  // namespace abo_host
