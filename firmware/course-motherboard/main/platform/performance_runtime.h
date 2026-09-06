#pragma once

#include <array>
#include <cstdint>

#include "abo_host/host_link.h"
#include "abo_p2/performance_controller.h"
#include "abo_p2/rhythm_led_model.h"
#include "abo_p2/score_snapshot.h"
#include "abo_p1/voice_engine.h"
#include "platform/instrument_voice_session.h"
#include "platform/led_strip_status.h"

namespace easy_input {

// Main-task adapter for the pure P2 controller. It owns the active score
// snapshot and is the only place where controller actions cross into P1
// audio, bank swapping, LEDs, and USB state messages.
class PerformanceRuntime {
 public:
  PerformanceRuntime(abo_host::HostLink& host,
                     InstrumentVoiceSession& voice,
                     StatusLedStrip& leds);

  bool begin();
  const abo_p2::PerformanceState& state() const { return controller_.state(); }

  bool set_mode(abo_p2::PerformanceMode mode, std::uint32_t now_ms);
  bool set_volume(std::uint8_t volume, std::uint32_t now_ms);
  bool reset_from_transport(std::uint32_t now_ms);
  void on_key(std::uint8_t key_index, bool pressed, std::uint32_t now_ms);
  void on_encoder_steps(int steps, std::uint32_t now_ms);
  void on_encoder_press(std::uint32_t now_ms);
  void advance_audio_frames(std::uint64_t frames, std::uint32_t now_ms);
  void poll(std::uint32_t now_ms);

 private:
  static constexpr std::uint8_t kInstrumentCount = 3U;
  static constexpr std::uint8_t kInvalidInstrument = 0xffU;

  bool refresh_score_at_standby();
  void execute_actions(std::uint32_t now_ms);
  void publish_state(std::uint32_t now_ms,
                     bool force = false,
                     bool send = true);
  void render_leds(std::uint32_t now_ms);
  void sync_led_phase(std::uint32_t now_ms);
  void fail_platform(const char* reason, std::uint32_t now_ms);
  bool request_instrument(std::uint8_t instrument);
  std::uint32_t elapsed_ms() const;

  abo_host::HostLink& host_;
  InstrumentVoiceSession& voice_;
  StatusLedStrip& leds_;
  abo_p2::ScoreSnapshot active_score_;
  abo_p2::PerformanceController controller_;
  abo_p2::RhythmLedModel rhythm_leds_;
  std::array<abo_p2::ScoreNote, abo_p2::ScoreSnapshot::kMaxNotes>
      converted_notes_{};
  std::uint32_t active_score_crc32_ = 0U;
  std::array<std::uint32_t, 7> note_generations_{};
  std::uint32_t next_generation_ = 0U;
  abo_p2::PerformancePhase rendered_phase_ = abo_p2::PerformancePhase::Standby;
  bool last_instrument_loading_ = false;
  bool state_published_ = false;
  std::uint32_t last_state_publish_ms_ = 0U;
  bool started_ = false;
};

}  // namespace easy_input
