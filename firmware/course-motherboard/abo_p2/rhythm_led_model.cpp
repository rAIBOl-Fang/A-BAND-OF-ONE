#include "abo_p2/rhythm_led_model.h"

#include <array>
#include <cstdint>

namespace abo_p2 {
namespace {

constexpr std::array<RhythmLedColor, RhythmLedModel::kLedCount> kRainbow{{
    {48U, 0U, 5U},
    {46U, 17U, 0U},
    {38U, 35U, 0U},
    {0U, 43U, 11U},
    {0U, 36U, 43U},
}};

constexpr std::array<std::size_t, RhythmLedModel::kFinishedFrameCount>
    kFinishedPath{{0U, 1U, 2U, 3U, 4U, 3U, 2U, 1U}};

}  // namespace

RhythmLedModel::RhythmLedModel() = default;

void RhythmLedModel::reset() {
  phase_ = RhythmLedPhase::Standby;
  beat_index_ = 0U;
  beat_valid_ = false;
  correct_until_ms_ = 0U;
  error_until_ms_ = 0U;
  finished_started_ms_ = 0U;
}

void RhythmLedModel::set_phase(RhythmLedPhase phase, std::uint32_t now_ms) {
  phase_ = phase;
  correct_until_ms_ = 0U;
  error_until_ms_ = 0U;

  if (phase == RhythmLedPhase::Finished) {
    finished_started_ms_ = now_ms;
  }
  if (phase != RhythmLedPhase::Playing) {
    beat_valid_ = false;
  }
}

void RhythmLedModel::on_beat_tick(std::uint8_t beat_index,
                                  std::uint32_t now_ms) {
  if (phase_ != RhythmLedPhase::Playing) {
    return;
  }
  beat_index_ = static_cast<std::uint8_t>(beat_index % kLedCount);
  beat_valid_ = true;
  (void)now_ms;
}

void RhythmLedModel::on_correct(std::uint32_t now_ms) {
  if (phase_ != RhythmLedPhase::Playing) {
    return;
  }
  correct_until_ms_ = now_ms + kCorrectPulseMs;
}

void RhythmLedModel::on_error(std::uint32_t now_ms) {
  if (phase_ != RhythmLedPhase::Playing) {
    return;
  }
  error_until_ms_ = now_ms + kErrorPulseMs;
}

RhythmLedFrame RhythmLedModel::frame_at(std::uint32_t now_ms) const {
  switch (phase_) {
    case RhythmLedPhase::Standby:
      return {};
    case RhythmLedPhase::Playing:
      return playing_frame(now_ms);
    case RhythmLedPhase::Paused:
      return RhythmLedFrame{{{kPausedAmber, kPausedAmber, kPausedAmber,
                              kPausedAmber, kPausedAmber}}};
    case RhythmLedPhase::Finished:
      return finished_frame_at(now_ms);
  }
  return {};
}

bool RhythmLedModel::before_deadline(std::uint32_t now_ms,
                                     std::uint32_t deadline_ms) {
  return static_cast<std::int32_t>(deadline_ms - now_ms) > 0;
}

RhythmLedFrame RhythmLedModel::playing_frame(std::uint32_t now_ms) const {
  RhythmLedFrame frame{};
  if (before_deadline(now_ms, error_until_ms_)) {
    for (auto& pixel : frame.pixels) {
      pixel = kRed;
    }
    return frame;
  }
  if (before_deadline(now_ms, correct_until_ms_)) {
    for (auto& pixel : frame.pixels) {
      pixel = kGreen;
    }
    return frame;
  }
  if (beat_valid_) {
    frame.pixels[beat_index_] = kBlue;
  }
  return frame;
}

RhythmLedFrame RhythmLedModel::finished_frame_at(
    std::uint32_t now_ms) const {
  const auto elapsed_ms = now_ms - finished_started_ms_;
  if (elapsed_ms >= kFinishedDurationMs) {
    return {};
  }

  const auto step = static_cast<std::size_t>(elapsed_ms / kFinishedStepMs);
  const auto head = kFinishedPath[step];
  RhythmLedFrame frame{};
  for (std::size_t index = 0U; index < kLedCount; ++index) {
    frame.pixels[index] = kRainbow[(index + head) % kRainbow.size()];
  }
  return frame;
}

}  // namespace abo_p2
