#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace abo_host {

struct WireStateView {
  const char* mode = "free";
  const char* phase = "standby";
  const char* subphase = "none";
  std::uint8_t instrument = 0;
  bool instrument_loading = false;
  std::uint8_t octave = 4;
  std::uint16_t bpm = 90;
  std::uint16_t errors = 0;
  std::uint16_t cursor = 0;
  std::uint32_t elapsed = 0;
  std::uint32_t elapsed_ms = 0;
  std::uint32_t score_crc32 = 0;
  std::uint32_t note_ticks = 0;
  std::uint32_t note_total_ticks = 0;
  std::int16_t knob = 0;
  std::uint8_t volume = 70;
  std::array<bool, 5> leds{};
  const char* const* keys = nullptr;
  std::size_t key_count = 0;
  bool score_loaded = false;
  std::string_view score_id{};
  std::uint8_t score_version = 0;
};

// Returns the byte count including the trailing newline, or 0 if the output
// buffer is too small or an argument is invalid. The output is NUL-terminated
// on success.
std::size_t format_ack(char* output,
                       std::size_t capacity,
                       std::string_view command,
                       std::string_view tx);

std::size_t format_score_commit_ack(char* output,
                                    std::size_t capacity,
                                    std::string_view tx,
                                    std::string_view score_id,
                                    std::uint8_t score_version);

std::size_t format_judge(char* output,
                         std::size_t capacity,
                         bool ok,
                         std::uint8_t target,
                         std::uint8_t expected,
                         std::uint16_t errors);

std::size_t format_result(char* output,
                          std::size_t capacity,
                          std::uint16_t errors,
                          std::uint32_t elapsed_ms);

std::size_t format_input(char* output,
                         std::size_t capacity,
                         std::uint32_t sequence,
                         std::string_view control,
                         std::string_view phase);

std::size_t format_state(char* output,
                         std::size_t capacity,
                         const WireStateView& state);

}  // namespace abo_host
