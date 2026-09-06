#include "abo_p0/startup_self_check.h"

namespace abo_p0 {

void StartupSelfCheck::start(bool cold_boot) {
  led_request_pending_ = cold_boot;
  speaker_request_pending_ = false;
  phase_ = cold_boot ? StartupSelfCheckPhase::LedRequested
                     : StartupSelfCheckPhase::Skipped;
}

void StartupSelfCheck::poll(bool led_ready,
                            bool speaker_ready,
                            bool speaker_busy) {
  if (phase_ == StartupSelfCheckPhase::LedRequested && led_ready &&
      speaker_ready && !speaker_busy) {
    speaker_request_pending_ = true;
    phase_ = StartupSelfCheckPhase::SpeakerRequested;
    return;
  }

  if (phase_ == StartupSelfCheckPhase::SpeakerRequested && speaker_busy) {
    phase_ = StartupSelfCheckPhase::WaitingSpeaker;
    return;
  }

  if (phase_ == StartupSelfCheckPhase::WaitingSpeaker && !speaker_busy) {
    phase_ = StartupSelfCheckPhase::Complete;
  }
}

bool StartupSelfCheck::take_led_request() {
  const bool requested = led_request_pending_;
  led_request_pending_ = false;
  return requested;
}

bool StartupSelfCheck::take_speaker_request() {
  const bool requested = speaker_request_pending_;
  speaker_request_pending_ = false;
  return requested;
}

StartupSelfCheckPhase StartupSelfCheck::phase() const {
  return phase_;
}

}  // namespace abo_p0
