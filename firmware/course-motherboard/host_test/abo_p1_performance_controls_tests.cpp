#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include "abo_p1/performance_controls.h"
#include "abo_p1/voice_engine.h"

namespace {

using abo_p1::Instrument;
using abo_p1::PerformanceActionType;
using abo_p1::PerformanceControls;
using abo_p1::VoiceEngine;

constexpr std::int16_t kMidiC4 = 60;
constexpr std::array<std::int16_t, 7> kPianoRoots{{47, 54, 60, 66, 72, 78, 84}};
constexpr std::array<std::int16_t, 7> kStringsRoots{{55, 60, 64, 67, 72, 76, 81}};
constexpr std::array<std::int16_t, 9> kClarinetRoots{{50, 53, 58, 62, 65, 70, 74, 77, 82}};
constexpr std::int16_t kSample = 1;

template <std::size_t N>
std::array<VoiceEngine::RootSample, N> roots_from_midi(
    const std::array<std::int16_t, N>& midis) {
  std::array<VoiceEngine::RootSample, N> roots{};
  for (std::size_t index = 0; index < N; ++index) {
    roots[index] = {midis[index], 2U, 0U, 2U, &kSample};
  }
  return roots;
}

std::int16_t expected_midi(std::uint8_t solfege, std::int8_t octave) {
  constexpr std::array<std::int16_t, 7> semitones{{0, 2, 4, 5, 7, 9, 11}};
  return static_cast<std::int16_t>(kMidiC4 + (octave - 4) * 12 +
                                  semitones[solfege]);
}

void note_keys_map_to_solfege_and_release_once() {
  PerformanceControls controls;

  for (std::uint8_t key = 0; key < 7; ++key) {
    const auto down = controls.on_key(key, true, key + 1U);
    assert(down.type == PerformanceActionType::NoteOn);
    assert(down.key_index == key);
    assert(down.solfege_index == key);
    assert(down.octave == 4);
    assert(down.instrument == Instrument::Piano);
    assert(controls.held_mask() == (1U << key));

    const auto duplicate = controls.on_key(key, true, 99U);
    assert(duplicate.type == PerformanceActionType::None);

    const auto up = controls.on_key(key, false, 99U);
    assert(up.type == PerformanceActionType::NoteOff);
    assert(up.key_index == key);
    assert(up.generation == key + 1U);
    assert(controls.held_mask() == 0U);
  }
}

void encoder_cycles_octaves_and_only_affects_next_note() {
  PerformanceControls controls;
  assert(controls.octave() == 4);
  assert(controls.knob_angle() == 0);

  const auto first = controls.on_key(0, true, 1U);
  assert(first.type == PerformanceActionType::NoteOn);
  assert(first.octave == 4);

  assert(controls.on_encoder_steps(1).type == PerformanceActionType::None);
  assert(controls.octave() == 5);
  assert(controls.knob_angle() == 45);
  assert(controls.on_encoder_steps(1).type == PerformanceActionType::None);
  assert(controls.octave() == 3);
  assert(controls.knob_angle() == -45);
  assert(controls.on_encoder_steps(-1).type == PerformanceActionType::None);
  assert(controls.octave() == 5);

  assert(controls.on_key(0, false, 1U).type == PerformanceActionType::NoteOff);
  const auto next = controls.on_key(0, true, 2U);
  assert(next.type == PerformanceActionType::NoteOn);
  assert(next.octave == 5);
  assert(controls.on_key(0, false, 2U).type == PerformanceActionType::NoteOff);

  assert(controls.on_encoder_steps(0).type == PerformanceActionType::None);
  assert(controls.on_encoder_press().type == PerformanceActionType::None);
}

void s8_cycles_instrument_on_press_only() {
  PerformanceControls controls;

  const auto strings = controls.on_key(7, true, 1U);
  assert(strings.type == PerformanceActionType::InstrumentChange);
  assert(strings.instrument == Instrument::Strings);
  assert(strings.previous_instrument == Instrument::Piano);
  assert(controls.on_key(7, true, 2U).type == PerformanceActionType::None);
  assert(controls.on_key(7, false, 2U).type == PerformanceActionType::None);

  const auto clarinet = controls.on_key(7, true, 3U);
  assert(clarinet.type == PerformanceActionType::InstrumentChange);
  assert(clarinet.instrument == Instrument::Clarinet);
  assert(clarinet.previous_instrument == Instrument::Strings);
  assert(controls.on_key(7, false, 3U).type == PerformanceActionType::None);

  const auto piano = controls.on_key(7, true, 4U);
  assert(piano.type == PerformanceActionType::InstrumentChange);
  assert(piano.instrument == Instrument::Piano);
  assert(piano.previous_instrument == Instrument::Clarinet);
}

template <std::size_t N>
void assert_all_21_notes(Instrument instrument,
                         const std::array<std::int16_t, N>& root_midis) {
  const auto roots = roots_from_midi(root_midis);
  PerformanceControls controls;
  controls.set_instrument(instrument);
  assert(controls.on_encoder_steps(-1).type == PerformanceActionType::None);
  for (std::int8_t octave : {3, 4, 5}) {
    if (octave != 3) {
      assert(controls.on_encoder_steps(1).type == PerformanceActionType::None);
    }
    for (std::uint8_t solfege = 0; solfege < 7; ++solfege) {
      const auto down = controls.on_key(solfege, true, 1U);
      assert(down.type == PerformanceActionType::NoteOn);
      assert(down.instrument == instrument);
      assert(down.octave == octave);
      assert(down.solfege_index == solfege);

      VoiceEngine engine;
      assert(engine.note_on(instrument, down.solfege_index, down.octave,
                            roots.data(), roots.size()));
      const auto target = expected_midi(solfege, octave);
      assert(engine.target_midi() == target);
      assert(engine.phase_step_q16() != 0U);

      const auto distance = static_cast<std::int16_t>(
          std::abs(static_cast<int>(engine.source_root_midi()) - target));
      if (instrument == Instrument::Strings && target < 55) {
        // The documented low-register physical-range exception.
        assert(distance <= 7);
      } else if (instrument == Instrument::Strings) {
        assert(distance <= 2);
      } else {
        assert(distance <= 3);
      }

      assert(controls.on_key(solfege, false, 1U).type ==
             PerformanceActionType::NoteOff);
    }
  }
}

std::string read_file(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void product_sources_use_queue_and_real_state_fields() {
  const auto session = read_file("../main/platform/instrument_voice_session.cpp");
  const auto session_header =
      read_file("../main/platform/instrument_voice_session.h");
  const auto app = read_file("../main/abo_app_main.cpp");
  const auto host = read_file("../abo_host/host_link.cpp");
  const auto wire = read_file("../abo_host/wire_messages.cpp");
  assert(session.find("command_queue_") != std::string::npos);
  assert(session.find("VoiceCommandType::NoteOn") != std::string::npos);
  assert(session.find("VoiceCommandType::NoteOff") != std::string::npos);
  assert(session_header.find("command_queue_") != std::string::npos);
  assert(app.find("platform/performance_runtime.h") != std::string::npos);
  assert(app.find("performance.on_key") != std::string::npos);
  assert(app.find("take_rendered_frames") != std::string::npos);
  assert(host.find("set_p2_state") != std::string::npos);
  assert(wire.find("instrument_loading") != std::string::npos);
  assert(wire.find("elapsed_ms") != std::string::npos);
  assert(wire.find("volume") != std::string::npos);
}

}  // namespace

int main() {
  note_keys_map_to_solfege_and_release_once();
  encoder_cycles_octaves_and_only_affects_next_note();
  s8_cycles_instrument_on_press_only();
  assert_all_21_notes(Instrument::Piano, kPianoRoots);
  assert_all_21_notes(Instrument::Strings, kStringsRoots);
  assert_all_21_notes(Instrument::Clarinet, kClarinetRoots);
  product_sources_use_queue_and_real_state_fields();
  return 0;
}
