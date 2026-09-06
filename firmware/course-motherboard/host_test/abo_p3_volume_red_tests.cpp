#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

std::string read_source(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void require_contains(const std::string& source,
                      const char* token,
                      const char* message,
                      bool* all_passed) {
  if (source.find(token) != std::string::npos) return;
  std::cerr << "RED: " << message << "\n";
  *all_passed = false;
}

void mixer_volume_contract_is_not_implemented(bool* all_passed) {
  const auto header = read_source("abo_p1/include/abo_p1/mono_voice_mixer.h");
  const auto source = read_source("abo_p1/mono_voice_mixer.cpp");

  require_contains(header, "set_volume",
                   "MonoVoiceMixer exposes a volume target setter",
                   all_passed);
  require_contains(source, "set_volume",
                   "MonoVoiceMixer applies volume targets",
                   all_passed);
  require_contains(source, "volume_ramp",
                   "MonoVoiceMixer uses a named volume ramp",
                   all_passed);
  require_contains(source, "100U",
                   "MonoVoiceMixer clamps the 0..100 volume boundary",
                   all_passed);
  require_contains(header, "kVolumeRampFrames = 480U",
                   "MonoVoiceMixer defines the 10ms/480-frame ramp",
                   all_passed);
}

void session_volume_mailbox_contract_is_not_implemented(bool* all_passed) {
  const auto header =
      read_source("main/platform/instrument_voice_session.h");
  const auto source =
      read_source("main/platform/instrument_voice_session.cpp");
  const auto runtime = read_source("main/platform/performance_runtime.cpp");

  require_contains(header, "set_volume",
                   "InstrumentVoiceSession exposes a runtime volume setter",
                   all_passed);
  require_contains(header, "target_volume",
                   "InstrumentVoiceSession owns a target-volume mailbox",
                   all_passed);
  require_contains(source, "mixer_.set_volume",
                   "only the audio consumer applies the volume to the mixer",
                   all_passed);
  require_contains(source, "volume_generation",
                   "volume mailbox updates are generation-tracked",
                   all_passed);
  require_contains(runtime, "set_volume",
                   "PerformanceRuntime forwards the volume request",
                   all_passed);
}

void realtime_volume_safety_contract_is_not_implemented(bool* all_passed) {
  const auto mixer_source = read_source("abo_p1/mono_voice_mixer.cpp");
  const auto session_header =
      read_source("main/platform/instrument_voice_session.h");
  const auto session_source =
      read_source("main/platform/instrument_voice_session.cpp");

  require_contains(mixer_source, "clamp_pcm16",
                   "volume scaling retains symmetric PCM16 saturation",
                   all_passed);
  require_contains(mixer_source, "crossfade_remaining_",
                   "volume scaling is applied during crossfade rendering",
                   all_passed);
  require_contains(session_header, "std::atomic",
                   "volume requests use an atomic audio-mailbox boundary",
                   all_passed);
  require_contains(session_source, "std::memory_order",
                   "volume mailbox access specifies memory ordering",
                   all_passed);
}

}  // namespace

int main() {
  bool all_passed = true;
  mixer_volume_contract_is_not_implemented(&all_passed);
  session_volume_mailbox_contract_is_not_implemented(&all_passed);
  realtime_volume_safety_contract_is_not_implemented(&all_passed);
  return all_passed ? 0 : 1;
}
