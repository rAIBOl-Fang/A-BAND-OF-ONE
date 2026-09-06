#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "abo_p2/performance_controller.h"

namespace {

using abo_p2::PerformanceActionType;
using abo_p2::PerformanceController;
using abo_p2::PerformanceMode;
using abo_p2::PerformancePhase;
using abo_p2::ScoreNote;
using abo_p2::ScoreView;

std::string read_source(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

void require(bool condition, const char* message, bool* all_passed) {
  if (condition) return;
  std::cerr << "RED: " << message << '\n';
  *all_passed = false;
}

void require_contains(const std::string& source,
                      const char* token,
                      const char* message,
                      bool* all_passed) {
  require(source.find(token) != std::string::npos, message, all_passed);
}

void state_wire_contract_is_not_implemented(bool* all_passed) {
  const auto header = read_source("abo_host/include/abo_host/wire_messages.h");
  const auto source = read_source("abo_host/wire_messages.cpp");

  require_contains(header, "score_crc32", "WireStateView declares score_crc32",
                   all_passed);
  require_contains(header, "note_ticks", "WireStateView declares note_ticks",
                   all_passed);
  require_contains(header, "note_total_ticks",
                   "WireStateView declares note_total_ticks", all_passed);
  require_contains(source, "score_crc32",
                   "format_state serializes score_crc32", all_passed);
  require_contains(source, "note_ticks",
                   "format_state serializes note_ticks", all_passed);
  require_contains(source, "note_total_ticks",
                   "format_state serializes note_total_ticks", all_passed);
}

void input_and_tx_contract_is_not_implemented(bool* all_passed) {
  const auto header = read_source("abo_host/include/abo_host/host_link.h");
  const auto source = read_source("abo_host/host_link.cpp");

  require(header.find("send_input") != std::string::npos ||
              header.find("InputEvent") != std::string::npos,
          "HostLink exposes an input event sender", all_passed);
  require_contains(source, "format_input",
                   "HostLink emits input press/release messages", all_passed);
  require_contains(source, "critical_tx_",
                   "HostLink owns a bounded critical TX queue", all_passed);
  require_contains(source, "transport", "HostLink parses transport reset",
                   all_passed);
}

void finished_press_is_consumed_and_returns_to_standby(bool* all_passed) {
  static constexpr std::array<ScoreNote, 1> kSingleNote{{{0U, 96U}}};
  static constexpr ScoreView kScore{"p3-finished-input", 60U,
                                    kSingleNote.data(), kSingleNote.size()};

  PerformanceController controller(kScore);
  require(controller.set_mode(PerformanceMode::Score),
          "score mode can be selected in standby", all_passed);
  require(controller.encoder_press() == PerformanceActionType::StateChanged,
          "score starts from standby", all_passed);
  require(controller.key_down(0U) == PerformanceActionType::Judge,
          "correct note is judged", all_passed);
  controller.advance_frames(48000U);
  require(controller.state().phase == PerformancePhase::Finished,
          "last note reaches finished", all_passed);

  const auto s1_action = controller.key_down(0U);
  require(controller.state().phase == PerformancePhase::Standby,
          "any finished key press returns to standby", all_passed);
  require(s1_action == PerformanceActionType::StateChanged,
          "finished key press is consumed without note/judge action",
          all_passed);

  PerformanceController s8_controller(kScore);
  require(s8_controller.set_mode(PerformanceMode::Score),
          "score can be selected for S8 boundary check", all_passed);
  require(s8_controller.encoder_press() == PerformanceActionType::StateChanged,
          "score can start for S8 boundary check", all_passed);
  require(s8_controller.key_down(0U) == PerformanceActionType::Judge,
          "S8 boundary setup note is judged", all_passed);
  s8_controller.advance_frames(48000U);
  require(s8_controller.state().phase == PerformancePhase::Finished,
          "last note reaches finished for S8 boundary check", all_passed);

  const auto s8_action = s8_controller.key_down(7U);
  require(s8_controller.state().phase == PerformancePhase::Standby,
          "finished S8 press returns to standby", all_passed);
  require(s8_action == PerformanceActionType::StateChanged,
          "finished S8 press does not switch instrument", all_passed);
}

}  // namespace

int main() {
  bool all_passed = true;
  state_wire_contract_is_not_implemented(&all_passed);
  input_and_tx_contract_is_not_implemented(&all_passed);
  finished_press_is_consumed_and_returns_to_standby(&all_passed);
  return all_passed ? 0 : 1;
}
