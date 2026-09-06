#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "speaker_assets/abo_p1_piano_sound.h"
#include "abo_p1/voice_renderer.h"

namespace easy_input {

class PianoVoiceSession {
 public:
  PianoVoiceSession();
  ~PianoVoiceSession();

  PianoVoiceSession(const PianoVoiceSession&) = delete;
  PianoVoiceSession& operator=(const PianoVoiceSession&) = delete;

  esp_err_t begin(const speaker_assets::AboP1PianoSound& sound);
  bool note_on(std::uint8_t key_index, std::int8_t octave);
  void note_off();
  bool ready() const { return ready_; }

  bool render_frame(std::int16_t* output,
                    std::size_t output_samples,
                    bool* finished);

 private:
  std::int16_t* pcm_ = nullptr;
  std::uint32_t sample_count_ = 0U;
  abo_p1::VoiceEngine::RootSample root_{};
  abo_p1::VoiceRenderer renderer_;
  std::atomic<bool> note_off_requested_{false};
  bool ready_ = false;
};

bool piano_voice_frame_provider(void* context,
                               std::int16_t* output,
                               std::size_t output_samples,
                               bool* finished);

}  // namespace easy_input
