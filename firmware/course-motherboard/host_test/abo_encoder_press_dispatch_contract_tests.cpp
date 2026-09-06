#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

#ifndef EASY_INPUT_REPO_ROOT
#error "EASY_INPUT_REPO_ROOT must identify the firmware repository"
#endif

namespace {

std::string read_source(const char* relative_path) {
  std::ifstream input(std::string(EASY_INPUT_REPO_ROOT) + "/" + relative_path,
                      std::ios::binary);
  assert(input.good());
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

void encoder_release_does_not_toggle_performance_state() {
  const auto source = read_source("main/abo_app_main.cpp");
  const auto begin = source.find("void handle_encoder_event(");
  const auto end = source.find("\nbool handle_input_event(", begin);
  assert(begin != std::string::npos);
  assert(end != std::string::npos);

  const auto handler = source.substr(begin, end - begin);
  const auto press_branch = handler.find(
      "event.input == ai_keyboard::InputId::EncoderPress");
  const auto press_guard = handler.find(
      "event.phase == ai_keyboard::InputPhase::Pressed", press_branch);
  const auto dispatch = handler.find("app->performance.on_encoder_press(",
                                    press_branch);
  assert(press_branch != std::string::npos);
  assert(press_guard != std::string::npos);
  assert(dispatch != std::string::npos);
  assert(press_guard < dispatch);
}

}  // namespace

int main() {
  encoder_release_does_not_toggle_performance_state();
  return 0;
}
