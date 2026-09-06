#pragma once

#include <cstddef>
#include <cstdint>

namespace abo_p2 {

enum class PerformanceMode : std::uint8_t {
  Score = 0,
  Free = 1,
};

enum class PerformancePhase : std::uint8_t {
  Standby = 0,
  Playing = 1,
  Finished = 3,
};

enum class PerformanceSubphase : std::uint8_t {
  None = 0,
  Waiting = 1,
  Holding = 2,
};

enum class PerformanceActionType : std::uint8_t {
  None = 0,
  NoteOn = 1,
  NoteOff = 2,
  LoadInstrument = 3,
  LedEvent = 4,
  Judge = 5,
  StateChanged = 6,
  Result = 7,
};

struct ScoreNote {
  std::uint8_t solfege;
  std::uint16_t ticks;
};

struct ScoreView {
  const char* id;
  std::uint16_t bpm;
  const ScoreNote* notes;
  std::size_t note_count;
};

struct PerformanceState {
  PerformanceMode mode = PerformanceMode::Free;
  PerformancePhase phase = PerformancePhase::Standby;
  PerformanceSubphase subphase = PerformanceSubphase::None;
  std::uint8_t instrument = 0U;
  std::uint8_t octave = 4U;
  std::uint8_t volume = 70U;
  std::uint16_t bpm = 90U;
  std::uint16_t cursor = 0U;
  std::uint16_t errors = 0U;
  std::uint64_t elapsed_frames = 0U;
  std::uint64_t note_frames = 0U;
  std::uint32_t score_ticks = 0U;
  std::uint8_t active_key = 0xffU;
  std::uint8_t pending_instrument = 0xffU;
};

struct PerformanceAction {
  PerformanceActionType type = PerformanceActionType::None;
  bool ok = false;
  std::uint8_t key_index = 0U;
  std::uint8_t expected_index = 0U;
  std::uint8_t instrument = 0U;
  std::uint8_t previous_instrument = 0U;
};

}  // namespace abo_p2
