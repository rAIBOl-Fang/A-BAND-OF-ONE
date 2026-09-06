#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace abo_p2 {

struct RhythmLedColor {
  std::uint8_t red = 0U;
  std::uint8_t green = 0U;
  std::uint8_t blue = 0U;
};

enum class RhythmLedPhase : std::uint8_t {
  Standby = 0,
  Playing = 1,
  Paused = 2,
  Finished = 3,
};

struct RhythmLedFrame {
  static constexpr std::size_t kLedCount = 5U;
  std::array<RhythmLedColor, kLedCount> pixels{};
};

class RhythmLedModel {
 public:
  static constexpr std::size_t kLedCount = RhythmLedFrame::kLedCount;
  static constexpr std::uint32_t kCorrectPulseMs = 80U;
  static constexpr std::uint32_t kErrorPulseMs = 120U;
  static constexpr std::uint32_t kFinishedStepMs = 80U;
  static constexpr std::size_t kFinishedFrameCount = (kLedCount * 2U) - 2U;
  static constexpr std::uint32_t kFinishedDurationMs =
      kFinishedStepMs * kFinishedFrameCount;

  inline static constexpr RhythmLedColor kBlue{0U, 0U, 48U};
  inline static constexpr RhythmLedColor kGreen{0U, 48U, 0U};
  inline static constexpr RhythmLedColor kRed{48U, 0U, 0U};
  inline static constexpr RhythmLedColor kPausedAmber{16U, 8U, 0U};

  RhythmLedModel();

  void reset();
  void set_phase(RhythmLedPhase phase, std::uint32_t now_ms);
  void on_beat_tick(std::uint8_t beat_index, std::uint32_t now_ms);
  void on_correct(std::uint32_t now_ms);
  void on_error(std::uint32_t now_ms);
  RhythmLedFrame frame_at(std::uint32_t now_ms) const;

 private:
  static bool before_deadline(std::uint32_t now_ms,
                              std::uint32_t deadline_ms);
  RhythmLedFrame playing_frame(std::uint32_t now_ms) const;
  RhythmLedFrame finished_frame_at(std::uint32_t now_ms) const;

  RhythmLedPhase phase_ = RhythmLedPhase::Standby;
  std::uint8_t beat_index_ = 0U;
  bool beat_valid_ = false;
  std::uint32_t correct_until_ms_ = 0U;
  std::uint32_t error_until_ms_ = 0U;
  std::uint32_t finished_started_ms_ = 0U;
};

}  // namespace abo_p2
