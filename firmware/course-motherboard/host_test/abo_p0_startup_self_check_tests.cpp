#include <cassert>

#include "abo_p0/startup_self_check.h"

namespace {

void cold_boot_requests_led_then_one_speaker_probe_then_completes() {
  abo_p0::StartupSelfCheck check;
  check.start(true);

  assert(check.take_led_request());
  assert(!check.take_led_request());
  assert(!check.take_speaker_request());

  check.poll(true, true, false);
  assert(check.take_speaker_request());
  assert(!check.take_speaker_request());

  check.poll(true, true, true);
  assert(check.phase() == abo_p0::StartupSelfCheckPhase::WaitingSpeaker);

  check.poll(true, true, false);
  assert(check.phase() == abo_p0::StartupSelfCheckPhase::Complete);
}

}  // namespace

int main() {
  cold_boot_requests_led_then_one_speaker_probe_then_completes();
  return 0;
}
