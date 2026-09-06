#include "abo_p2/performance_controller.h"

#include <algorithm>

namespace abo_p2 {

PerformanceController::PerformanceController(ScoreView score)
    : score_(score), score_clock_(score.bpm) {
  state_.bpm = score_.bpm == 0U ? 90U : score_.bpm;
}

bool PerformanceController::set_mode(PerformanceMode mode) {
  if (state_.phase != PerformancePhase::Standby) {
    return false;
  }
  if (state_.mode == mode) {
    return true;
  }
  state_.mode = mode;
  state_.subphase = PerformanceSubphase::None;
  state_.cursor = 0U;
  state_.errors = 0U;
  state_.elapsed_frames = 0U;
  state_.note_frames = 0U;
  state_.score_ticks = 0U;
  state_.active_key = 0xffU;
  state_.pending_instrument = 0xffU;
  held_mask_ = 0U;
  current_key_ = 0U;
  score_clock_.reset(state_.bpm);
  return true;
}

bool PerformanceController::set_score(ScoreView score) {
  if (state_.phase != PerformancePhase::Standby || score.id == nullptr ||
      score.notes == nullptr || score.note_count == 0U) {
    return false;
  }
  score_ = score;
  state_.bpm = score_.bpm == 0U ? 90U : score_.bpm;
  state_.cursor = 0U;
  state_.errors = 0U;
  state_.elapsed_frames = 0U;
  state_.note_frames = 0U;
  state_.score_ticks = 0U;
  state_.active_key = 0xffU;
  state_.pending_instrument = 0xffU;
  score_clock_.reset(state_.bpm);
  return true;
}

PerformanceActionType PerformanceController::encoder_steps(int steps) {
  if (steps == 0 || state_.phase != PerformancePhase::Playing) {
    return PerformanceActionType::None;
  }

  const int direction = steps > 0 ? 1 : -1;
  const int count = steps > 0 ? steps : -steps;
  for (int index = 0; index < count; ++index) {
    const std::uint8_t octave = state_.octave == 3U
                                    ? 3U
                                    : (state_.octave == 4U ? 4U : 5U);
    const int next = (static_cast<int>(octave) - 3 + direction + 3) % 3;
    state_.octave = static_cast<std::uint8_t>(3 + next);
  }
  push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
  return PerformanceActionType::StateChanged;
}

PerformanceActionType PerformanceController::encoder_press() {
  switch (state_.phase) {
    case PerformancePhase::Standby:
      state_.phase = PerformancePhase::Playing;
      state_.subphase = state_.mode == PerformanceMode::Score
                            ? PerformanceSubphase::Waiting
                            : PerformanceSubphase::None;
      push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
      return PerformanceActionType::StateChanged;

    case PerformancePhase::Playing: {
      bool had_note = false;
      if (state_.mode == PerformanceMode::Score &&
          state_.subphase == PerformanceSubphase::Holding) {
        push_action({PerformanceActionType::NoteOff, false, current_key_, 0U});
        had_note = true;
      } else if (state_.mode == PerformanceMode::Free) {
        for (std::uint8_t key_index = 0U; key_index < 7U; ++key_index) {
          const auto bit = static_cast<std::uint8_t>(1U << key_index);
          if ((held_mask_ & bit) == 0U) continue;
          push_action(
              {PerformanceActionType::NoteOff, false, key_index, 0U});
          had_note = true;
        }
      }
      held_mask_ = 0U;
      current_key_ = 0U;
      state_.active_key = 0xffU;
      state_.phase = PerformancePhase::Standby;
      state_.subphase = state_.mode == PerformanceMode::Score
                            ? PerformanceSubphase::Waiting
                            : PerformanceSubphase::None;
      state_.note_frames = 0U;
      state_.score_ticks = 0U;
      state_.pending_instrument = 0xffU;
      score_clock_.reset(state_.bpm);
      push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
      return had_note ? PerformanceActionType::NoteOff
                      : PerformanceActionType::StateChanged;
    }

    case PerformancePhase::Finished:
      reset_from_finished();
      push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
      return PerformanceActionType::StateChanged;
  }
  return PerformanceActionType::None;
}

bool PerformanceController::reset_from_finished() {
  if (state_.phase != PerformancePhase::Finished) return false;
  state_.phase = PerformancePhase::Standby;
  state_.subphase = PerformanceSubphase::None;
  state_.cursor = 0U;
  state_.errors = 0U;
  state_.elapsed_frames = 0U;
  state_.note_frames = 0U;
  state_.score_ticks = 0U;
  state_.pending_instrument = 0xffU;
  state_.active_key = 0xffU;
  score_clock_.reset(state_.bpm);
  held_mask_ = 0U;
  current_key_ = 0U;
  return true;
}

bool PerformanceController::set_volume(std::uint8_t volume) {
  if (volume > 100U) volume = 100U;
  state_.volume = volume;
  return true;
}

void PerformanceController::platform_failure() {
  if (state_.phase != PerformancePhase::Playing) return;
  held_mask_ = 0U;
  current_key_ = 0U;
  state_.phase = PerformancePhase::Standby;
  state_.subphase = state_.mode == PerformanceMode::Score
                        ? PerformanceSubphase::Waiting
                        : PerformanceSubphase::None;
  state_.note_frames = 0U;
  state_.score_ticks = 0U;
  state_.pending_instrument = 0xffU;
  state_.active_key = 0xffU;
  score_clock_.reset(state_.bpm);
  push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
}

void PerformanceController::restore_instrument(std::uint8_t instrument) {
  if (instrument >= 3U) return;
  state_.instrument = instrument;
  state_.pending_instrument = 0xffU;
}

PerformanceActionType PerformanceController::cycle_instrument() {
  const auto base = state_.pending_instrument == 0xffU
                        ? state_.instrument
                        : state_.pending_instrument;
  const auto target = next_instrument(base);
  const bool note_active =
      state_.mode == PerformanceMode::Score
          ? state_.subphase == PerformanceSubphase::Holding
          : note_is_held();
  if (state_.phase == PerformancePhase::Playing && note_active) {
    state_.pending_instrument = target;
    push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
    return PerformanceActionType::None;
  }

  const auto previous = state_.instrument;
  state_.instrument = target;
  state_.pending_instrument = 0xffU;
  push_action({PerformanceActionType::LoadInstrument,
               false,
               0U,
               0U,
               target,
               previous});
  push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
  return PerformanceActionType::LoadInstrument;
}

PerformanceActionType PerformanceController::key_down(
    std::uint8_t key_index) {
  if (key_index >= 8U) {
    return PerformanceActionType::None;
  }

  if (state_.phase == PerformancePhase::Finished) {
    reset_from_finished();
    push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
    return PerformanceActionType::StateChanged;
  }

  const std::uint8_t bit = static_cast<std::uint8_t>(1U << key_index);
  if ((held_mask_ & bit) != 0U) {
    return PerformanceActionType::None;
  }
  held_mask_ = static_cast<std::uint8_t>(held_mask_ | bit);

  if (key_index == 7U) {
    return cycle_instrument();
  }

  if (state_.phase != PerformancePhase::Playing) {
    held_mask_ = static_cast<std::uint8_t>(held_mask_ & ~bit);
    return PerformanceActionType::None;
  }

  if (state_.mode == PerformanceMode::Free) {
    push_action({PerformanceActionType::NoteOn, false, key_index, 0U});
    return PerformanceActionType::NoteOn;
  }

  if (state_.subphase != PerformanceSubphase::Waiting ||
      state_.cursor >= score_.note_count) {
    return PerformanceActionType::None;
  }

  const std::uint8_t expected = score_.notes[state_.cursor].solfege;
  const bool correct = key_index == expected;
  if (!correct) {
    ++state_.errors;
  }
  push_action({PerformanceActionType::Judge, correct, key_index, expected});
  if (correct) {
    state_.subphase = PerformanceSubphase::Holding;
    current_key_ = key_index;
    state_.active_key = key_index;
    state_.note_frames = 0U;
    state_.score_ticks = 0U;
    score_clock_.reset(state_.bpm);
    push_action({PerformanceActionType::NoteOn, false, key_index, expected});
  }
  return PerformanceActionType::Judge;
}

PerformanceActionType PerformanceController::key_up(std::uint8_t key_index) {
  if (key_index >= 8U) {
    return PerformanceActionType::None;
  }
  if (state_.phase == PerformancePhase::Finished) {
    return PerformanceActionType::None;
  }
  const std::uint8_t bit = static_cast<std::uint8_t>(1U << key_index);
  if ((held_mask_ & bit) == 0U) {
    return PerformanceActionType::None;
  }
  held_mask_ = static_cast<std::uint8_t>(held_mask_ & ~bit);

  if (state_.phase == PerformancePhase::Playing &&
      state_.mode == PerformanceMode::Free) {
    push_action({PerformanceActionType::NoteOff, false, key_index, 0U});
    if (!note_is_held()) apply_pending_instrument();
    return PerformanceActionType::NoteOff;
  }
  if (key_index == 7U && !note_is_held()) apply_pending_instrument();
  return PerformanceActionType::None;
}

void PerformanceController::advance_frames(std::uint32_t frames) {
  if (state_.phase != PerformancePhase::Playing) {
    return;
  }

  if (state_.mode == PerformanceMode::Free) {
    state_.elapsed_frames += frames;
    return;
  }

  state_.elapsed_frames += frames;
  if (state_.subphase != PerformanceSubphase::Holding ||
      state_.cursor >= score_.note_count) {
    return;
  }

  state_.note_frames += frames;
  state_.score_ticks +=
      static_cast<std::uint32_t>(score_clock_.advance(frames));
  if (state_.score_ticks < score_.notes[state_.cursor].ticks) {
    return;
  }
  finish_current_note();
}

std::size_t PerformanceController::drain_actions(PerformanceAction* output,
                                                 std::size_t capacity) {
  if (output == nullptr || capacity == 0U) {
    return 0U;
  }
  const std::size_t count = std::min(action_count_, capacity);
  for (std::size_t index = 0U; index < count; ++index) {
    output[index] = actions_[index];
  }
  for (std::size_t index = count; index < action_count_; ++index) {
    actions_[index - count] = actions_[index];
  }
  action_count_ -= count;
  return count;
}

void PerformanceController::push_action(PerformanceAction action) {
  if (action_count_ >= kActionCapacity) {
    ++state_.errors;
    return;
  }
  actions_[action_count_++] = action;
}

void PerformanceController::finish_current_note() {
  push_action({PerformanceActionType::NoteOff, false, current_key_, 0U});
  current_key_ = 0U;
  state_.active_key = 0xffU;
  apply_pending_instrument();
  ++state_.cursor;
  state_.note_frames = 0U;
  state_.score_ticks = 0U;
  score_clock_.reset(state_.bpm);
  if (state_.cursor >= score_.note_count) {
    state_.phase = PerformancePhase::Finished;
    state_.subphase = PerformanceSubphase::None;
    held_mask_ = 0U;
    push_action({PerformanceActionType::Result, true, 0U, 0U});
    push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
    return;
  }
  state_.subphase = PerformanceSubphase::Waiting;
  push_action({PerformanceActionType::StateChanged, false, 0U, 0U});
}

void PerformanceController::apply_pending_instrument() {
  if (state_.pending_instrument == 0xffU) return;
  const auto previous = state_.instrument;
  state_.instrument = state_.pending_instrument;
  state_.pending_instrument = 0xffU;
  push_action({PerformanceActionType::LoadInstrument,
               false,
               0U,
               0U,
               state_.instrument,
               previous});
}

std::uint8_t PerformanceController::next_instrument(
    std::uint8_t instrument) {
  return static_cast<std::uint8_t>((instrument + 1U) % 3U);
}

bool PerformanceController::note_is_held() const {
  return (held_mask_ & 0x7fU) != 0U;
}

}  // namespace abo_p2
