#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "abo_p1/mono_voice_mixer.h"
#include "abo_p1/voice_command_queue.h"
#include "esp_err.h"
#include "speaker_assets/abo_p1_sound_bank.h"

namespace easy_input {

// Platform owner for the P1 instrument bank. The immutable Flash payloads
// stay in factory Flash; this object owns exactly one decoded PSRAM arena and
// exposes only the audio-consumer provider to SpeakerOutput.
class InstrumentVoiceSession {
 public:
  static constexpr std::size_t kFrameSamples = 480U;
  static constexpr std::size_t kMaxRoots = 9U;
  static constexpr std::size_t kMaxPcmSamples = 701257U;

  InstrumentVoiceSession();
  ~InstrumentVoiceSession();

  InstrumentVoiceSession(const InstrumentVoiceSession&) = delete;
  InstrumentVoiceSession& operator=(const InstrumentVoiceSession&) = delete;

  esp_err_t begin(const speaker_assets::AboP1EncodedBank& bank);
  bool ready() const { return ready_; }

  // Called by the main task. The actual arena overwrite is deferred until
  // the provider publishes bank_safe at a frame boundary.
  bool request_bank(const speaker_assets::AboP1EncodedBank* bank);
  bool poll_bank_swap();
  bool bank_loading() const {
    return bank_swap_requested_.load(std::memory_order_acquire) ||
           bank_loading_.load(std::memory_order_acquire) ||
           bank_safe_.load(std::memory_order_acquire);
  }
  bool bank_safe() const { return bank_safe_.load(std::memory_order_acquire); }
  esp_err_t last_bank_error() const {
    return static_cast<esp_err_t>(last_bank_error_.load(
        std::memory_order_acquire));
  }
  std::uint8_t current_instrument_index() const;

  bool note_on(std::uint8_t key_index,
               std::uint32_t generation,
               abo_p1::Instrument instrument,
               std::uint8_t solfege_index,
               std::int8_t octave);
  bool note_off(std::uint8_t key_index, std::uint32_t generation);

  // The main task writes one packed target; only render_frame consumes it.
  // Volume is intentionally a RAM-only control and does not touch assets,
  // bank ownership, or the I2S lifetime.
  bool set_volume(std::uint8_t volume) noexcept;
  std::uint8_t target_volume() const noexcept;

  // The provider always fills a complete block. It returns finished only
  // after request_exit() and the final voice release have completed.
  bool render_frame(std::int16_t* output,
                    std::size_t output_samples,
                    bool* finished);
  std::uint64_t take_rendered_frames() {
    return rendered_frames_.exchange(0U, std::memory_order_acq_rel);
  }
  void request_exit() { exit_requested_.store(true, std::memory_order_release); }

 private:
  esp_err_t load_bank(const speaker_assets::AboP1EncodedBank& bank);
  esp_err_t decode_bank(const speaker_assets::AboP1EncodedBank& bank,
                        abo_p1::VoiceEngine::RootSample* roots,
                        std::size_t* root_count);
  void apply_pending_commands();
  void apply_pending_volume(bool force = false) noexcept;

  static constexpr std::uint8_t kDefaultVolume = 70U;
  static constexpr std::uint32_t kVolumeMask = 0xffU;
  static std::uint32_t pack_volume_target(std::uint32_t generation,
                                          std::uint8_t volume) noexcept {
    return (generation << 8U) | volume;
  }

  std::int16_t* arena_ = nullptr;
  std::size_t arena_samples_ = 0U;
  std::array<abo_p1::VoiceEngine::RootSample, kMaxRoots> roots_{};
  std::size_t root_count_ = 0U;
  abo_p1::VoiceCommandQueue command_queue_;
  const speaker_assets::AboP1EncodedBank* current_bank_ = nullptr;
  const speaker_assets::AboP1EncodedBank* pending_bank_ = nullptr;
  abo_p1::MonoVoiceMixer mixer_;
  std::atomic<bool> bank_swap_requested_{false};
  std::atomic<bool> bank_safe_{false};
  std::atomic<bool> bank_loading_{false};
  std::atomic<bool> bank_resume_{false};
  std::atomic<bool> exit_requested_{false};
  std::atomic<std::uint64_t> rendered_frames_{0U};
  std::atomic<std::int32_t> last_bank_error_{ESP_OK};
  std::atomic<std::uint32_t> target_volume_mailbox_{
      pack_volume_target(0U, kDefaultVolume)};
  std::uint32_t volume_generation_ = 0U;
  bool audio_waiting_for_bank_ = false;
  bool ready_ = false;
};

bool instrument_voice_frame_provider(void* context,
                                     std::int16_t* output,
                                     std::size_t output_samples,
                                     bool* finished);

}  // namespace easy_input
