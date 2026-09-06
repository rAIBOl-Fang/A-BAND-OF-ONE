#include <array>
#include <cassert>
#include <cstdint>

#include "abo_p2/performance_controller.h"

namespace {

using abo_p2::PerformanceActionType;
using abo_p2::PerformanceController;
using abo_p2::PerformanceMode;
using abo_p2::PerformancePhase;
using abo_p2::PerformanceSubphase;
using abo_p2::ScoreNote;
using abo_p2::ScoreView;

constexpr std::array<ScoreNote, 2> kScale{{
    {0U, 96U},
    {1U, 96U},
}};

constexpr ScoreView kScaleScore{
    "p2-scale", 60U, kScale.data(), kScale.size()};

template <std::size_t N>
std::size_t drain(PerformanceController& controller,
                  std::array<abo_p2::PerformanceAction, N>& actions) {
  return controller.drain_actions(actions.data(), actions.size());
}

void starts_score_mode_and_reaches_holding() {
  PerformanceController controller(kScaleScore);
  assert(controller.state().phase == PerformancePhase::Standby);

  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.state().phase == PerformancePhase::Playing);
  assert(controller.state().subphase == PerformanceSubphase::Waiting);

  assert(controller.key_down(0U) == PerformanceActionType::Judge);
  assert(controller.state().subphase == PerformanceSubphase::Holding);
  assert(controller.state().cursor == 0U);
  assert(controller.state().errors == 0U);
  assert(controller.key_down(1U) == PerformanceActionType::None);
  assert(controller.state().errors == 0U);
}

void strict_judgement_keeps_cursor_and_counts_errors() {
  PerformanceController controller(kScaleScore);
  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);

  for (int attempt = 0; attempt < 3; ++attempt) {
    assert(controller.key_down(1U) == PerformanceActionType::Judge);
    assert(controller.key_up(1U) == PerformanceActionType::None);
  }
  assert(controller.state().cursor == 0U);
  assert(controller.state().errors == 3U);

  controller.advance_frames(96000U);
  assert(controller.state().elapsed_frames == 96000U);
  assert(controller.state().cursor == 0U);

  assert(controller.key_down(0U) == PerformanceActionType::Judge);
  assert(controller.state().subphase == PerformanceSubphase::Holding);
  controller.advance_frames(48000U);
  assert(controller.state().cursor == 1U);
  assert(controller.state().subphase == PerformanceSubphase::Waiting);
}

void pause_enters_standby_and_resume_restarts_current_note() {
  PerformanceController controller(kScaleScore);
  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.key_down(0U) == PerformanceActionType::Judge);
  controller.advance_frames(24000U);

  const auto elapsed_before_pause = controller.state().elapsed_frames;
  assert(controller.encoder_press() == PerformanceActionType::NoteOff);
  assert(controller.state().phase == PerformancePhase::Standby);
  assert(controller.state().subphase == PerformanceSubphase::Waiting);

  controller.advance_frames(48000U);
  assert(controller.state().elapsed_frames == elapsed_before_pause);
  assert(controller.state().cursor == 0U);

  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.state().phase == PerformancePhase::Playing);
  assert(controller.state().subphase == PerformanceSubphase::Waiting);
  assert(controller.key_down(0U) == PerformanceActionType::Judge);

  PerformanceController free_controller(kScaleScore);
  assert(free_controller.set_mode(PerformanceMode::Free));
  assert(free_controller.state().mode == PerformanceMode::Free);
  assert(free_controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(free_controller.key_down(0U) == PerformanceActionType::NoteOn);
  assert(free_controller.state().phase == PerformancePhase::Playing);
  assert(free_controller.state().mode == PerformanceMode::Free);
  assert(free_controller.key_down(0U) == PerformanceActionType::None);
  assert(free_controller.key_up(0U) == PerformanceActionType::NoteOff);
  assert(free_controller.key_down(0U) == PerformanceActionType::NoteOn);
  const auto free_pause_action = free_controller.encoder_press();
  assert(free_pause_action == PerformanceActionType::NoteOff);
  assert(free_controller.state().phase == PerformancePhase::Standby);
  assert(free_controller.key_down(0U) == PerformanceActionType::None);
  assert(free_controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(free_controller.state().phase == PerformancePhase::Playing);
  assert(free_controller.key_down(0U) == PerformanceActionType::NoteOn);
  assert(free_controller.state().errors == 0U);
}

void standby_rotation_does_not_switch_mode() {
  PerformanceController controller(kScaleScore);
  assert(controller.state().mode == PerformanceMode::Free);
  assert(controller.encoder_steps(1) == PerformanceActionType::None);
  assert(controller.state().mode == PerformanceMode::Free);
  assert(controller.encoder_steps(-1) == PerformanceActionType::None);
  assert(controller.state().mode == PerformanceMode::Free);

  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.state().mode == PerformanceMode::Score);
  assert(controller.encoder_steps(1) == PerformanceActionType::None);
  assert(controller.state().mode == PerformanceMode::Score);
}

void free_pause_releases_every_held_note() {
  PerformanceController controller(kScaleScore);
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  for (std::uint8_t key_index = 0U; key_index < 7U; ++key_index) {
    assert(controller.key_down(key_index) == PerformanceActionType::NoteOn);
  }

  std::array<abo_p2::PerformanceAction, 16> actions{};
  drain(controller, actions);
  assert(controller.encoder_press() == PerformanceActionType::NoteOff);
  assert(controller.state().phase == PerformancePhase::Standby);

  const auto action_count = drain(controller, actions);
  std::array<bool, 7> released{};
  std::size_t note_off_count = 0U;
  for (std::size_t index = 0U; index < action_count; ++index) {
    if (actions[index].type != PerformanceActionType::NoteOff) continue;
    assert(actions[index].key_index < released.size());
    released[actions[index].key_index] = true;
    ++note_off_count;
  }
  assert(note_off_count == 7U);
  for (const bool was_released : released) assert(was_released);
  assert(controller.state().errors == 0U);
}

void web_mode_change_is_rejected_while_playing_and_resets_at_standby() {
  PerformanceController controller(kScaleScore);
  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.state().phase == PerformancePhase::Playing);
  assert(!controller.set_mode(PerformanceMode::Free));
  assert(controller.state().mode == PerformanceMode::Score);

  assert(controller.key_down(1U) == PerformanceActionType::Judge);
  assert(controller.key_up(1U) == PerformanceActionType::None);
  controller.advance_frames(24000U);
  assert(controller.state().errors == 1U);
  assert(controller.state().elapsed_frames == 24000U);

  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.state().phase == PerformancePhase::Standby);
  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.state().errors == 1U);
  assert(controller.state().elapsed_frames == 24000U);

  assert(controller.set_mode(PerformanceMode::Free));
  assert(controller.state().mode == PerformanceMode::Free);
  assert(controller.state().cursor == 0U);
  assert(controller.state().errors == 0U);
  assert(controller.state().elapsed_frames == 0U);
  assert(controller.state().subphase == PerformanceSubphase::None);
}

void final_note_enters_finished_and_emits_result() {
  static constexpr std::array<ScoreNote, 1> kSingleNote{{{0U, 96U}}};
  static constexpr ScoreView score{"single", 60U, kSingleNote.data(),
                                    kSingleNote.size()};
  PerformanceController controller(score);
  assert(controller.set_mode(PerformanceMode::Score));
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.key_down(0U) == PerformanceActionType::Judge);
  controller.advance_frames(48000U);

  assert(controller.state().phase == PerformancePhase::Finished);
  std::array<abo_p2::PerformanceAction, 16> actions{};
  const auto action_count = drain(controller, actions);
  bool saw_result = false;
  for (std::size_t index = 0U; index < action_count; ++index) {
    saw_result = saw_result ||
                 actions[index].type == PerformanceActionType::Result;
  }
  assert(saw_result);

  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.state().phase == PerformancePhase::Standby);
  assert(controller.encoder_press() == PerformanceActionType::StateChanged);
  assert(controller.key_down(0U) == PerformanceActionType::Judge);
}

}  // namespace

int main() {
  starts_score_mode_and_reaches_holding();
  strict_judgement_keeps_cursor_and_counts_errors();
  pause_enters_standby_and_resume_restarts_current_note();
  standby_rotation_does_not_switch_mode();
  free_pause_releases_every_held_note();
  web_mode_change_is_rejected_while_playing_and_resets_at_standby();
  final_note_enters_finished_and_emits_result();
  return 0;
}
