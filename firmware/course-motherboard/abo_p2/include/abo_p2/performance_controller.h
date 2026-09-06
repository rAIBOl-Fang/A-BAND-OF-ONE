#pragma once

#include <cstddef>
#include <cstdint>

#include "abo_p2/performance_clock.h"
#include "abo_p2/performance_types.h"

namespace abo_p2 {

class PerformanceController {
 public:
  static constexpr std::size_t kActionCapacity = 16U;

  explicit PerformanceController(ScoreView score);

  const PerformanceState& state() const { return state_; }

  bool set_mode(PerformanceMode mode);
  bool set_score(ScoreView score);
  bool set_volume(std::uint8_t volume);
  void platform_failure();
  bool reset_from_finished();
  void restore_instrument(std::uint8_t instrument);
  PerformanceActionType encoder_steps(int steps);
  PerformanceActionType encoder_press();
  PerformanceActionType cycle_instrument();
  PerformanceActionType key_down(std::uint8_t key_index);
  PerformanceActionType key_up(std::uint8_t key_index);
  void advance_frames(std::uint32_t frames);

  std::size_t drain_actions(PerformanceAction* output,
                            std::size_t capacity);

 private:
  static constexpr std::uint64_t kFramesPerMinute = 48000ULL * 60ULL;
  static constexpr std::uint16_t kTicksPerQuarter = 96U;

  void push_action(PerformanceAction action);
  void finish_current_note();
  void apply_pending_instrument();
  static std::uint8_t next_instrument(std::uint8_t instrument);
  bool note_is_held() const;

  ScoreView score_{};
  PerformanceState state_{};
  PerformanceClock score_clock_{};
  PerformanceAction actions_[kActionCapacity]{};
  std::size_t action_count_ = 0U;
  std::uint8_t held_mask_ = 0U;
  std::uint8_t current_key_ = 0U;
};

}  // namespace abo_p2
