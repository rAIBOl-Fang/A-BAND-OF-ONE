#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "abo_p1/voice_renderer.h"

namespace abo_p1 {

// Owns the current voice and one retiring voice. A note-on never replaces
// the current renderer in place: the old voice is faded out while the new
// voice fades in, so the I2S producer keeps receiving contiguous frames.
class MonoVoiceMixer {
 public:
  static constexpr std::uint8_t kDefaultVolume = 70U;
  static constexpr std::uint32_t kVolumeRampFrames = 480U;

  struct Config {
    std::uint32_t attack_frames = 1U;
    std::uint32_t release_frames = 1U;
    std::uint32_t crossfade_frames = 480U;  // 10 ms at 48 kHz.
  };

  MonoVoiceMixer();
  explicit MonoVoiceMixer(Config config);

  bool note_on(std::uint8_t key_index,
               std::uint32_t generation,
               Instrument instrument,
               std::uint8_t solfege_index,
               std::int8_t octave,
               const VoiceRenderer::RootSample* roots,
               std::size_t root_count);
  bool note_off(std::uint8_t key_index, std::uint32_t generation);
  void set_volume(std::uint8_t volume) noexcept;
  void reset() noexcept;

  std::size_t render(std::int16_t* output, std::size_t frame_count);

  bool active() const;
  std::size_t active_voice_count() const;
  std::uint8_t current_key_index() const { return current_key_index_; }
  std::uint32_t current_generation() const { return current_generation_; }
  bool crossfade_active() const { return crossfade_remaining_ != 0U; }
  std::uint32_t crossfade_frames_remaining() const {
    return crossfade_remaining_;
  }
  std::uint8_t volume() const { return current_volume_; }
  std::uint8_t target_volume() const { return target_volume_; }
  bool volume_ramp_active() const {
    return volume_ramp_elapsed_ < kVolumeRampFrames;
  }

 private:
  static constexpr std::int32_t kVolumeUnityQ15 = 32767;

  static std::int16_t clamp_pcm16(std::int64_t sample);
  static std::uint8_t normalize_volume(std::uint8_t volume);
  static std::int32_t volume_to_gain_q15(std::uint8_t volume);
  std::int16_t apply_volume(std::int64_t sample) const;
  void advance_volume_ramp() noexcept;
  void retire_old_voice();

  Config config_{};
  std::array<VoiceRenderer, 2U> voices_{};
  std::array<std::uint8_t, 2U> key_indices_{};
  std::array<std::uint32_t, 2U> generations_{};
  std::size_t current_slot_ = 0U;
  std::size_t old_slot_ = 1U;
  std::uint8_t current_key_index_ = 0U;
  std::uint32_t current_generation_ = 0U;
  std::uint32_t crossfade_remaining_ = 0U;
  std::uint8_t current_volume_ = kDefaultVolume;
  std::uint8_t target_volume_ = kDefaultVolume;
  std::int32_t current_volume_gain_q15_ = kVolumeUnityQ15;
  std::int32_t volume_ramp_start_gain_q15_ = kVolumeUnityQ15;
  std::int32_t target_volume_gain_q15_ = kVolumeUnityQ15;
  std::uint32_t volume_ramp_elapsed_ = kVolumeRampFrames;
  bool has_current_ = false;
};

}  // namespace abo_p1
