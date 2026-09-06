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
  const auto app = read_file("../main/abo_app_main.cpp");
  const auto main_cmake = read_file("../main/CMakeLists.txt");
  const auto session_header = read_file(
      "../main/platform/piano_voice_session.h");
  const auto session_source = read_file(
      "../main/platform/piano_voice_session.cpp");

  contains(session_header, "class PianoVoiceSession");
  contains(session_source, "heap_caps_malloc");
  contains(session_source, "SoundAssetStreamDecoder");
  contains(session_header, "VoiceRenderer");
  contains(app, "PianoVoiceSession");
  contains(app, "request_voice_stream");
  contains(app, "piano_voice_frame_provider");
  contains(main_cmake, "platform/piano_voice_session.cpp");
  return 0;
}
