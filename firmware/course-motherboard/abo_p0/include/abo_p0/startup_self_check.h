#pragma once

namespace abo_p0 {

enum class StartupSelfCheckPhase {
  Idle,
  LedRequested,
  SpeakerRequested,
  WaitingSpeaker,
  Complete,
  Skipped,
};

class StartupSelfCheck {
 public:
  void start(bool cold_boot);
  void poll(bool led_ready, bool speaker_ready, bool speaker_busy);

  bool take_led_request();
  bool take_speaker_request();
  StartupSelfCheckPhase phase() const;

 private:
  StartupSelfCheckPhase phase_ = StartupSelfCheckPhase::Idle;
  bool led_request_pending_ = false;
  bool speaker_request_pending_ = false;
};

}  // namespace abo_p0
