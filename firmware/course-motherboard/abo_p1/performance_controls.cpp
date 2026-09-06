#include "abo_p1/performance_controls.h"

namespace abo_p1 {

PerformanceAction PerformanceControls::on_key(std::uint8_t key_index,
                                              bool pressed,
                                              std::uint32_t generation) {
  if (key_index < kNoteKeyCount) {
    const auto mask = static_cast<std::uint32_t>(1U << key_index);
    if (pressed) {
      if ((held_mask_ & mask) != 0U) return {};
      held_mask_ |= mask;
      generations_[key_index] = generation;
      return {PerformanceActionType::NoteOn, key_index, key_index,
              static_cast<std::int8_t>(octave()),
              generation, instrument_, instrument_};
    }
    if ((held_mask_ & mask) == 0U) return {};
    held_mask_ &= ~mask;
    return {PerformanceActionType::NoteOff,
            key_index,
            key_index,
            static_cast<std::int8_t>(octave()),
            generations_[key_index],
            instrument_,
            instrument_};
  }

  if (key_index != kInstrumentKey) return {};
  if (!pressed) {
    instrument_key_down_ = false;
    return {};
  }
  if (instrument_key_down_) return {};
  instrument_key_down_ = true;
  const auto previous = instrument_;
  instrument_ = next_instrument(instrument_);
  return {PerformanceActionType::InstrumentChange,
          key_index,
          0U,
          static_cast<std::int8_t>(octave()),
          generation,
          instrument_,
          previous};
}

PerformanceAction PerformanceControls::on_encoder_steps(int steps) {
  if (steps == 0) return {};
  const int direction = steps > 0 ? 1 : -1;
  for (int remaining = steps > 0 ? steps : -steps; remaining > 0;
       --remaining) {
    const auto count = static_cast<int>(octaves_.size());
    octave_index_ = static_cast<std::uint8_t>(
        (static_cast<int>(octave_index_) + direction + count) % count);
  }
  return {};
}

PerformanceAction PerformanceControls::on_encoder_press() const {
  return {};
}

Instrument PerformanceControls::next_instrument(Instrument instrument) {
  switch (instrument) {
    case Instrument::Piano: return Instrument::Strings;
    case Instrument::Strings: return Instrument::Clarinet;
    case Instrument::Clarinet: return Instrument::Piano;
  }
  return Instrument::Piano;
}

}  // namespace abo_p1
