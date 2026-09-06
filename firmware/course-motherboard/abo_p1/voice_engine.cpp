#include "abo_p1/voice_engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace abo_p1 {
namespace {

constexpr std::uint32_t kQ16One = 1U << 16U;
constexpr std::int16_t kHighestSolfegeIndex = 6;
constexpr std::array<std::int16_t, 7> kSolfegeSemitones{{
    0, 2, 4, 5, 7, 9, 11,
}};

std::uint32_t saturating_q16(double ratio) {
  const auto scaled = ratio * static_cast<double>(kQ16One);
  if (scaled <= 0.0) return 1U;
  if (scaled >= static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return static_cast<std::uint32_t>(std::lround(scaled));
}

}  // namespace

VoiceEngine::VoiceEngine()
    : VoiceEngine(Config{}) {}

VoiceEngine::VoiceEngine(Config config)
    : config_(config) {
  config_.attack_frames =
      std::max<std::uint32_t>(1U, config_.attack_frames);
  config_.release_frames =
      std::max<std::uint32_t>(1U, config_.release_frames);
}

void VoiceEngine::reset() noexcept {
  phase_ = VoicePhase::Idle;
  instrument_ = Instrument::Piano;
  target_midi_ = 0;
  source_root_midi_ = 0;
  source_frame_count_ = 0U;
  loop_start_frame_ = 0U;
  loop_end_frame_ = 0U;
  phase_step_q16_ = 0U;
  source_cursor_q16_ = 0U;
  phase_frames_ = 0U;
}

bool VoiceEngine::note_on(Instrument instrument,
                          std::uint8_t solfege_index,
                          std::int8_t octave,
                          const RootSample* roots,
                          std::size_t root_count) {
  if (solfege_index > static_cast<std::uint8_t>(kHighestSolfegeIndex) ||
      roots == nullptr || root_count == 0U || octave < 0) {
    return false;
  }

  const auto target = static_cast<std::int16_t>(
      60 + (static_cast<std::int16_t>(octave) - 4) * 12 +
      kSolfegeSemitones[solfege_index]);
  std::size_t nearest = 0U;
  auto nearest_distance = std::numeric_limits<std::int16_t>::max();
  for (std::size_t index = 0U; index < root_count; ++index) {
    const auto distance = static_cast<std::int16_t>(
        std::abs(static_cast<int>(roots[index].midi) - target));
    if (distance < nearest_distance ||
        (distance == nearest_distance && roots[index].midi < roots[nearest].midi)) {
      nearest = index;
      nearest_distance = distance;
    }
  }

  const auto& root = roots[nearest];
  if (root.frame_count == 0U || root.loop_end_frame > root.frame_count ||
      (root.loop_end_frame != 0U &&
       root.loop_start_frame >= root.loop_end_frame)) {
    return false;
  }

  instrument_ = instrument;
  target_midi_ = target;
  source_root_midi_ = root.midi;
  source_frame_count_ = root.frame_count;
  loop_start_frame_ = root.loop_start_frame;
  loop_end_frame_ = root.loop_end_frame == 0U
                        ? root.frame_count
                        : root.loop_end_frame;
  phase_step_q16_ = pitch_step_q16(target_midi_, source_root_midi_);
  source_cursor_q16_ = 0U;
  phase_frames_ = 0U;
  phase_ = VoicePhase::Attack;
  return true;
}

void VoiceEngine::note_off() {
  if (phase_ == VoicePhase::Attack || phase_ == VoicePhase::Sustain) {
    phase_ = VoicePhase::Release;
    phase_frames_ = 0U;
  }
}

void VoiceEngine::advance(std::uint32_t frames) {
  if (phase_ == VoicePhase::Idle || frames == 0U) return;
  if (phase_ == VoicePhase::Attack) {
    if (frames < config_.attack_frames - phase_frames_) {
      phase_frames_ += frames;
      return;
    }
    frames -= config_.attack_frames - phase_frames_;
    phase_ = VoicePhase::Sustain;
    phase_frames_ = 0U;
  }
  if (phase_ == VoicePhase::Release) {
    if (frames >= config_.release_frames - phase_frames_) {
      phase_ = VoicePhase::Idle;
      phase_frames_ = 0U;
    } else {
      phase_frames_ += frames;
    }
    return;
  }
  if (phase_ == VoicePhase::Sustain) phase_frames_ = 0U;
}

void VoiceEngine::advance_source(std::uint32_t phase_delta_q16) {
  if (phase_ == VoicePhase::Idle || source_frame_count_ == 0U) return;
  source_cursor_q16_ += phase_delta_q16;
  const auto loop_start_q16 = static_cast<std::uint64_t>(loop_start_frame_) << 16U;
  const auto loop_end_q16 = static_cast<std::uint64_t>(loop_end_frame_) << 16U;
  if (loop_end_q16 <= loop_start_q16 || source_cursor_q16_ < loop_end_q16) {
    return;
  }
  const auto loop_length_q16 = loop_end_q16 - loop_start_q16;
  const auto wrapped = loop_start_q16 +
                       (source_cursor_q16_ - loop_start_q16) %
                           loop_length_q16;
  source_cursor_q16_ = wrapped;
}

bool VoiceEngine::set_source_cursor(std::uint64_t cursor_q16) {
  if (source_frame_count_ == 0U ||
      cursor_q16 >=
          (static_cast<std::uint64_t>(source_frame_count_) << 16U)) {
    return false;
  }
  source_cursor_q16_ = cursor_q16;
  return true;
}

std::uint32_t VoiceEngine::pitch_step_q16(
    std::int16_t target_midi,
    std::int16_t source_root_midi) {
  const auto semitones = static_cast<double>(target_midi) - source_root_midi;
  return saturating_q16(std::pow(2.0, semitones / 12.0));
}

}  // namespace abo_p1
