#pragma once

#include <array>
#include <cstdint>

#include "abo_p1/voice_engine.h"

namespace abo_p1 {

enum class PerformanceActionType : std::uint8_t {
  None,
  NoteOn,
  NoteOff,
  InstrumentChange,
};

struct PerformanceAction {
  PerformanceActionType type = PerformanceActionType::None;
  std::uint8_t key_index = 0U;
  std::uint8_t solfege_index = 0U;
  std::int8_t octave = 4;
  std::uint32_t generation = 0U;
  Instrument instrument = Instrument::Piano;
  Instrument previous_instrument = Instrument::Piano;
};

// Pure product controls. It deliberately knows no GPIO, audio, USB, or
// FreeRTOS details, so the exact key/knob contract stays host-testable.
class PerformanceControls {
 public:
  static constexpr std::uint8_t kNoteKeyCount = 7U;
  static constexpr std::uint8_t kInstrumentKey = 7U;

  PerformanceAction on_key(std::uint8_t key_index,
                           bool pressed,
                           std::uint32_t generation);
  PerformanceAction on_encoder_steps(int steps);
  PerformanceAction on_encoder_press() const;

  std::uint8_t octave() const { return octaves_[octave_index_]; }
  std::int16_t knob_angle() const {
    return static_cast<std::int16_t>((static_cast<int>(octave()) - 4) * 45);
  }
  Instrument instrument() const { return instrument_; }
  std::uint32_t held_mask() const { return held_mask_; }

  // Used only when a bank request is rejected before it reaches the audio
  // boundary; it restores the logical state to the actual loaded bank.
  void set_instrument(Instrument instrument) { instrument_ = instrument; }

 private:
  static Instrument next_instrument(Instrument instrument);

  static constexpr std::array<std::uint8_t, 3> octaves_{{3U, 4U, 5U}};
  std::array<std::uint32_t, kNoteKeyCount> generations_{};
  std::uint32_t held_mask_ = 0U;
  std::uint8_t octave_index_ = 1U;
  Instrument instrument_ = Instrument::Piano;
  bool instrument_key_down_ = false;
};

}  // namespace abo_p1
