#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const char* path) {
  std::ifstream input(path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void contains(const std::string& contents, const char* needle) {
  assert(contents.find(needle) != std::string::npos);
}

}  // namespace

int main() {
  const auto session_header = read_file(
      "../main/platform/instrument_voice_session.h");
  const auto session_source = read_file(
      "../main/platform/instrument_voice_session.cpp");
  const auto app = read_file("../main/abo_app_main.cpp");
  const auto main_cmake = read_file("../main/CMakeLists.txt");

  contains(session_header, "kMaxPcmSamples = 701257U");
  contains(session_header, "kMaxRoots = 9U");
  contains(session_header, "bank_safe");
  contains(session_header, "request_bank");
  contains(session_header, "instrument_voice_frame_provider");
  contains(session_source, "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT");
  contains(session_source, "SoundAssetStreamDecoder");
  contains(session_source, "AboP1RootStorage::EiadV1");
  contains(session_source, "AboP1RootStorage::Pcm16Le");
  contains(session_source, "payload_bytes");
  contains(session_source, "sizeof(std::int16_t)");
  contains(session_source, "std::memcpy");
  contains(session_source, "bank_safe");
  contains(session_source, "load_bank");
  contains(session_source, "bank_load_failed");
  contains(session_source, "esp_timer_get_time");
  contains(session_source, "heap_caps_get_free_size");
  contains(session_source, "heap_caps_get_largest_free_block");
  contains(session_source, "bank_load_ms");
  contains(session_source, "ESP_LOGW");
  contains(session_source, "P1_DIAG");
  contains(session_source, "std::fill");
  contains(session_source, "finished");

  assert(app.find("PianoVoiceSession") == std::string::npos);
  contains(app, "InstrumentVoiceSession");
  contains(app, "request_voice_stream");
  contains(main_cmake, "platform/instrument_voice_session.cpp");
  assert(main_cmake.find("platform/piano_voice_session.cpp") ==
         std::string::npos);
  return 0;
}
