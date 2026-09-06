#pragma once

#include <cstddef>
#include <cstdint>

#include "speaker_assets/abo_p1_piano_sound.h"

namespace easy_input::speaker_assets {

enum class AboP1RootStorage : std::uint8_t {
  EiadV1,
  Pcm16Le,
};

struct AboP1EncodedRoot {
  const char* id = nullptr;
  AboP1RootStorage storage = AboP1RootStorage::EiadV1;
  std::uint8_t root_midi = 0U;
  const std::uint8_t* payload = nullptr;
  std::size_t payload_bytes = 0U;
  std::uint32_t decoded_samples = 0U;
  std::uint32_t loop_start_sample = 0U;
  std::uint32_t loop_end_sample = 0U;
};

struct AboP1EncodedBank {
  std::uint8_t instrument_index = 0U;
  const AboP1EncodedRoot* roots = nullptr;
  std::uint8_t root_count = 0U;
  std::uint32_t decoded_samples_total = 0U;
};

inline constexpr std::uint8_t kAboP1PianoInstrument = 0U;
inline constexpr std::uint8_t kAboP1ViolinInstrument = 1U;
inline constexpr std::uint8_t kAboP1ClarinetInstrument = 2U;

[[nodiscard]] const AboP1EncodedBank* abo_p1_sound_bank(
    std::uint8_t instrument_index);

// Compatibility view for the already validated P0/P1 piano C4 startup path.
[[nodiscard]] AboP1PianoSound abo_p1_piano_c4_sound();

}  // namespace easy_input::speaker_assets
