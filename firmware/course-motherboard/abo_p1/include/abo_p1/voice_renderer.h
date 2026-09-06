#pragma once

#include <cstddef>
#include <cstdint>

#include "abo_p1/voice_engine.h"

namespace abo_p1 {

// Host-testable PCM renderer for one active voice. The platform render pump
// will call render() for each continuous I2S block; it does not stop the
// audio clock at sustain-loop boundaries.
class VoiceRenderer {
 public:
  using RootSample = VoiceEngine::RootSample;

  struct Config {
    std::uint32_t attack_frames = 1U;
    std::uint32_t release_frames = 1U;
  };

  VoiceRenderer();
  explicit VoiceRenderer(Config config);

  bool note_on(Instrument instrument,
               std::uint8_t solfege_index,
               std::int8_t octave,
               const RootSample* roots,
               std::size_t root_count);
  void reset() noexcept;
  void note_off();

  // Renders mono PCM frames. The return value is always frame_count for a
  // valid output buffer; an Idle voice contributes zeros rather than a gap.
  std::size_t render(std::int16_t* output, std::size_t frame_count);

  VoicePhase phase() const { return voice_.phase(); }
  bool active() const { return phase() != VoicePhase::Idle; }
  const VoiceEngine& voice() const { return voice_; }

 private:
  const RootSample* find_selected_root(const RootSample* roots,
                                       std::size_t root_count) const;
  std::int16_t sample_at_cursor() const;
  std::int32_t envelope_q15() const;

  VoiceEngine voice_;
  const RootSample* selected_root_ = nullptr;
};

}  // namespace abo_p1
