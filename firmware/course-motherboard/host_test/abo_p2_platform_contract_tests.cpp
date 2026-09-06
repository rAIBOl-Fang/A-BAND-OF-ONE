#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

#include "abo_host/score_protocol.h"
#include "abo_p2/performance_controller.h"
#include "abo_p2/score_snapshot.h"

namespace {

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void require_contains(const std::string& text, const char* needle) {
  assert(!text.empty());
  assert(text.find(needle) != std::string::npos);
}

void test_runtime_has_platform_seams() {
  const auto header = read_file("main/platform/performance_runtime.h");
  const auto source = read_file("main/platform/performance_runtime.cpp");
  const auto app = read_file("main/abo_app_main.cpp");
  const auto cmake = read_file("main/CMakeLists.txt");

  require_contains(header, "ScoreSnapshot");
  require_contains(header, "on_key");
  require_contains(header, "advance_audio_frames");
  require_contains(source, "committed_score");
  require_contains(source, "canonical_crc32");
  require_contains(header, "active_score_crc32_");
  require_contains(source, "set_score");
  require_contains(source, "request_bank");
  require_contains(source, "show_rhythm_frame");
  require_contains(source, "send_judge");
  require_contains(source, "send_result");
  require_contains(source, "advance_frames");
  require_contains(app, "platform/performance_runtime.h");
  require_contains(app, "performance.advance_audio_frames");
  require_contains(app, "take_rendered_frames");
  require_contains(cmake, "platform/performance_runtime.cpp");
  assert(source.find("vTaskDelay") == std::string::npos);
}

void test_snapshot_copies_score_storage() {
  std::array<abo_p2::ScoreNote, 2> source{{{0U, 96U}, {1U, 192U}}};
  abo_p2::ScoreSnapshot snapshot;
  assert(snapshot.assign("first", 90U, source.data(), source.size()));
  const auto view = snapshot.view();
  assert(view.id != nullptr);
  assert(std::string(view.id) == "first");
  assert(view.bpm == 90U);
  assert(view.note_count == 2U);

  source[0].solfege = 6U;
  source[0].ticks = 768U;
  assert(view.notes[0].solfege == 0U);
  assert(view.notes[0].ticks == 96U);
}

void test_controller_replaces_score_only_at_standby() {
  constexpr std::array<abo_p2::ScoreNote, 1> first{{{0U, 96U}}};
  constexpr std::array<abo_p2::ScoreNote, 1> next{{{1U, 96U}}};
  abo_p2::ScoreSnapshot first_snapshot;
  abo_p2::ScoreSnapshot next_snapshot;
  assert(first_snapshot.assign("first", 60U, first.data(), first.size()));
  assert(next_snapshot.assign("next", 120U, next.data(), next.size()));

  abo_p2::PerformanceController controller(first_snapshot.view());
  assert(controller.set_mode(abo_p2::PerformanceMode::Score));
  assert(controller.encoder_press() ==
         abo_p2::PerformanceActionType::StateChanged);
  assert(!controller.set_score(next_snapshot.view()));
  assert(controller.key_down(0U) == abo_p2::PerformanceActionType::Judge);

  assert(controller.encoder_press() == abo_p2::PerformanceActionType::NoteOff);
  assert(controller.state().phase == abo_p2::PerformancePhase::Standby);
  assert(controller.set_score(next_snapshot.view()));
  assert(controller.key_down(1U) == abo_p2::PerformanceActionType::None);
  assert(controller.encoder_press() == abo_p2::PerformanceActionType::StateChanged);
  assert(controller.key_down(1U) == abo_p2::PerformanceActionType::Judge);
}

void test_protocol_crc_changes_when_note_payload_changes() {
  abo_host::Score first{};
  first.score_version = 1U;
  first.id = {'s', 'a', 'm', 'e', '-', 'i', 'd'};
  first.id_length = 7U;
  first.title_utf8 = {'S', 'a', 'm', 'e', ' ', 'I', 'D'};
  first.title_length = 7U;
  first.bpm = 90U;
  first.note_count = 1U;
  first.notes[0] = {0U, 96U};
  auto changed = first;
  changed.notes[0].n = 6U;

  const auto first_crc = abo_host::ScoreProtocol::canonical_crc32(first);
  const auto changed_crc = abo_host::ScoreProtocol::canonical_crc32(changed);
  assert(first_crc != changed_crc);
}

void test_encoder_and_instrument_action_contract() {
  constexpr std::array<abo_p2::ScoreNote, 1> note{{{0U, 96U}}};
  abo_p2::ScoreSnapshot snapshot;
  assert(snapshot.assign("scale", 60U, note.data(), note.size()));
  abo_p2::PerformanceController controller(snapshot.view());

  assert(controller.encoder_steps(1) == abo_p2::PerformanceActionType::None);
  assert(controller.state().mode == abo_p2::PerformanceMode::Free);
  assert(controller.set_mode(abo_p2::PerformanceMode::Score));
  assert(controller.encoder_press() ==
         abo_p2::PerformanceActionType::StateChanged);
  assert(controller.encoder_steps(1) ==
         abo_p2::PerformanceActionType::StateChanged);
  assert(controller.state().octave == 5U);

  assert(controller.cycle_instrument() ==
         abo_p2::PerformanceActionType::LoadInstrument);
  assert(controller.state().instrument == 1U);

  controller.encoder_press();
  assert(controller.state().phase == abo_p2::PerformancePhase::Standby);
  assert(controller.encoder_press() ==
         abo_p2::PerformanceActionType::StateChanged);
  assert(controller.key_down(0U) == abo_p2::PerformanceActionType::Judge);
  assert(controller.cycle_instrument() ==
         abo_p2::PerformanceActionType::None);
  assert(controller.state().pending_instrument == 2U);
  controller.advance_frames(48000U);

  std::array<abo_p2::PerformanceAction, 16> actions{};
  const auto count = controller.drain_actions(actions.data(), actions.size());
  bool saw_deferred_load = false;
  for (std::size_t i = 0U; i < count; ++i) {
    saw_deferred_load = saw_deferred_load ||
                        actions[i].type == abo_p2::PerformanceActionType::LoadInstrument;
  }
  assert(saw_deferred_load);
  assert(controller.state().instrument == 2U);
  assert(controller.state().pending_instrument == 0xffU);
}

}  // namespace

int main() {
  test_runtime_has_platform_seams();
  test_snapshot_copies_score_storage();
  test_controller_replaces_score_only_at_standby();
  test_protocol_crc_changes_when_note_payload_changes();
  test_encoder_and_instrument_action_contract();
  return 0;
}
