#include "abo_p1/mono_voice_mixer.h"

#include <algorithm>

namespace abo_p1 {
namespace {

constexpr std::int32_t kMixerQ15One = 32767;

MonoVoiceMixer::Config normalize_config(MonoVoiceMixer::Config config) {
  config.attack_frames = std::max<std::uint32_t>(1U, config.attack_frames);
  config.release_frames = std::max<std::uint32_t>(1U, config.release_frames);
  config.crossfade_frames =
      std::max<std::uint32_t>(1U, config.crossfade_frames);
  return config;
}

}  // namespace

MonoVoiceMixer::MonoVoiceMixer()
    : MonoVoiceMixer(Config{}) {}

MonoVoiceMixer::MonoVoiceMixer(Config config)
    : config_(normalize_config(config)),
      voices_{
          VoiceRenderer(VoiceRenderer::Config{
              config_.attack_frames, config_.release_frames}),
          VoiceRenderer(VoiceRenderer::Config{
              config_.attack_frames, config_.release_frames}),
      } {}

bool MonoVoiceMixer::note_on(
    std::uint8_t key_index,
    std::uint32_t generation,
    Instrument instrument,
    std::uint8_t solfege_index,
    std::int8_t octave,
    const VoiceRenderer::RootSample* roots,
    std::size_t root_count) {
  if (has_current_ && !voices_[current_slot_].active() &&
      crossfade_remaining_ == 0U) {
    has_current_ = false;
  }

  const auto new_slot = has_current_ ? 1U - current_slot_ : current_slot_;
  if (!voices_[new_slot].note_on(
          instrument, solfege_index, octave, roots, root_count)) {
    return false;
  }

  if (has_current_) {
    old_slot_ = current_slot_;
    voices_[old_slot_].note_off();
    crossfade_remaining_ = config_.crossfade_frames;
  } else {
    crossfade_remaining_ = 0U;
  }

  current_slot_ = new_slot;
  key_indices_[current_slot_] = key_index;
  generations_[current_slot_] = generation;
  current_key_index_ = key_index;
  current_generation_ = generation;
  has_current_ = true;
  return true;
}

bool MonoVoiceMixer::note_off(std::uint8_t key_index,
                              std::uint32_t generation) {
  if (!has_current_ || key_indices_[current_slot_] != key_index ||
      generations_[current_slot_] != generation) {
    return false;
  }
  voices_[current_slot_].note_off();
  return true;
}

void MonoVoiceMixer::set_volume(std::uint8_t volume) noexcept {
  const auto normalized = normalize_volume(volume);
  if (normalized == target_volume_) return;

  target_volume_ = normalized;
  volume_ramp_start_gain_q15_ = current_volume_gain_q15_;
  target_volume_gain_q15_ = volume_to_gain_q15(normalized);
  volume_ramp_elapsed_ = 0U;
}

void MonoVoiceMixer::reset() noexcept {
  voices_[0].reset();
  voices_[1].reset();
  current_slot_ = 0U;
  old_slot_ = 1U;
  current_key_index_ = 0U;
  current_generation_ = 0U;
  crossfade_remaining_ = 0U;
  current_volume_ = kDefaultVolume;
  target_volume_ = kDefaultVolume;
  current_volume_gain_q15_ = kVolumeUnityQ15;
  volume_ramp_start_gain_q15_ = kVolumeUnityQ15;
  target_volume_gain_q15_ = kVolumeUnityQ15;
  volume_ramp_elapsed_ = kVolumeRampFrames;
  has_current_ = false;
}

std::size_t MonoVoiceMixer::render(std::int16_t* output,
                                   std::size_t frame_count) {
  if (output == nullptr) return 0U;

  for (std::size_t index = 0U; index < frame_count; ++index) {
    advance_volume_ramp();
    std::int16_t current_sample = 0;
    if (has_current_) {
      voices_[current_slot_].render(&current_sample, 1U);
    }

    if (has_current_ && crossfade_remaining_ != 0U) {
      std::int16_t old_sample = 0;
      voices_[old_slot_].render(&old_sample, 1U);
      const auto remaining =
          static_cast<std::uint64_t>(crossfade_remaining_);
      const auto total =
          static_cast<std::uint64_t>(config_.crossfade_frames);
      const auto elapsed = total - remaining;
      const auto old_gain = remaining * kMixerQ15One / total;
      const auto new_gain = (elapsed + 1LL) * kMixerQ15One / total;
      const auto old_scaled = static_cast<std::int32_t>(
          static_cast<std::int32_t>(old_sample) *
          static_cast<std::int32_t>(old_gain) / kMixerQ15One);
      const auto new_scaled = static_cast<std::int32_t>(
          static_cast<std::int32_t>(current_sample) *
          static_cast<std::int32_t>(new_gain) / kMixerQ15One);
      const auto mixed = static_cast<std::int32_t>(old_scaled + new_scaled);
      output[index] = apply_volume(mixed);

      --crossfade_remaining_;
      if (crossfade_remaining_ == 0U) {
        retire_old_voice();
      }
    } else {
      output[index] = apply_volume(current_sample);
    }

    if (has_current_ && !voices_[current_slot_].active() &&
        crossfade_remaining_ == 0U) {
      has_current_ = false;
    }
  }
  return frame_count;
}

bool MonoVoiceMixer::active() const {
  return has_current_ &&
         (crossfade_remaining_ != 0U || voices_[current_slot_].active());
}

std::size_t MonoVoiceMixer::active_voice_count() const {
  if (!has_current_) return 0U;
  return crossfade_remaining_ == 0U ? 1U : 2U;
}

std::int16_t MonoVoiceMixer::clamp_pcm16(std::int64_t sample) {
  return static_cast<std::int16_t>(std::clamp<std::int64_t>(
      sample, static_cast<std::int64_t>(-32768),
      static_cast<std::int64_t>(32767)));
}

std::uint8_t MonoVoiceMixer::normalize_volume(std::uint8_t volume) {
  return volume > 100U ? 100U : volume;
}

std::int32_t MonoVoiceMixer::volume_to_gain_q15(std::uint8_t volume) {
  return static_cast<std::int32_t>(
      (static_cast<std::int64_t>(volume) * kVolumeUnityQ15) /
      kDefaultVolume);
}

std::int16_t MonoVoiceMixer::apply_volume(std::int64_t sample) const {
  const auto scaled =
      (sample * current_volume_gain_q15_) / kVolumeUnityQ15;
  return clamp_pcm16(scaled);
}

void MonoVoiceMixer::advance_volume_ramp() noexcept {
  if (volume_ramp_elapsed_ >= kVolumeRampFrames) return;

  ++volume_ramp_elapsed_;
  const auto elapsed = static_cast<std::int64_t>(volume_ramp_elapsed_);
  const auto delta = static_cast<std::int64_t>(target_volume_gain_q15_) -
                     volume_ramp_start_gain_q15_;
  current_volume_gain_q15_ = static_cast<std::int32_t>(
      static_cast<std::int64_t>(volume_ramp_start_gain_q15_) +
      (delta * elapsed) / kVolumeRampFrames);
  if (volume_ramp_elapsed_ == kVolumeRampFrames) {
    current_volume_gain_q15_ = target_volume_gain_q15_;
    current_volume_ = target_volume_;
  }
}

void MonoVoiceMixer::retire_old_voice() {
  voices_[old_slot_].reset();
}

}  // namespace abo_p1
