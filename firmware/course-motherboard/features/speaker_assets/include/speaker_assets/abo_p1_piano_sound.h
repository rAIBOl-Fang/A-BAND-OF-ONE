#pragma once

#include <cstddef>
#include <cstdint>

namespace easy_input::speaker_assets {

struct AboP1PianoSound {
  const std::uint8_t* encoded = nullptr;
  std::size_t encoded_bytes = 0U;
  // Sustain loop window in EIAD frames, pinned by the P1 asset manifest.
  // The end frame is exclusive; the decoder clamps it to the actual sample
  // count when the last EIAD frame is partial.
  std::uint16_t loop_start_frame = 0U;
  std::uint16_t loop_end_frame = 0U;
};

[[nodiscard]] AboP1PianoSound abo_p1_piano_c4_sound();

}  // namespace easy_input::speaker_assets
