#pragma once

#include <cstddef>
#include <cstdint>

namespace abo_p1 {

enum class Instrument : std::uint8_t {
  Piano,
  Strings,
  Clarinet,
};

enum class VoicePhase : std::uint8_t {
  Idle,
  Attack,
  Sustain,
  Release,
};

class VoiceEngine {
 public:
  struct Config {
    std::uint32_t attack_frames = 1U;
    std::uint32_t release_frames = 1U;
  };

  struct RootSample {
    std::int16_t midi = 0;
    std::uint32_t frame_count = 0U;
    std::uint32_t loop_start_frame = 0U;
    std::uint32_t loop_end_frame = 0U;
    const std::int16_t* samples = nullptr;
  };

  VoiceEngine();
  explicit VoiceEngine(Config config);

  bool note_on(Instrument instrument,
               std::uint8_t solfege_index,
               std::int8_t octave,
               const RootSample* roots,
               std::size_t root_count);
  void reset() noexcept;
  void note_off();
  void advance(std::uint32_t frames);
  void advance_source(std::uint32_t phase_delta_q16);

  bool set_source_cursor(std::uint64_t cursor_q16);
  VoicePhase phase() const { return phase_; }
  Instrument instrument() const { return instrument_; }
  std::int16_t target_midi() const { return target_midi_; }
  std::int16_t source_root_midi() const { return source_root_midi_; }
  std::uint32_t phase_step_q16() const { return phase_step_q16_; }
  std::uint64_t source_cursor_q16() const { return source_cursor_q16_; }
  std::uint32_t phase_frames() const { return phase_frames_; }
  std::uint32_t attack_frames() const { return config_.attack_frames; }
  std::uint32_t release_frames() const { return config_.release_frames; }

 private:
  static std::uint32_t pitch_step_q16(std::int16_t target_midi,
                                      std::int16_t source_root_midi);

  Config config_;
  VoicePhase phase_ = VoicePhase::Idle;
  Instrument instrument_ = Instrument::Piano;
  std::int16_t target_midi_ = 0;
  std::int16_t source_root_midi_ = 0;
  std::uint32_t source_frame_count_ = 0U;
  std::uint32_t loop_start_frame_ = 0U;
  std::uint32_t loop_end_frame_ = 0U;
  std::uint32_t phase_step_q16_ = 0U;
  std::uint64_t source_cursor_q16_ = 0U;
  std::uint32_t phase_frames_ = 0U;
};

}  // namespace abo_p1
