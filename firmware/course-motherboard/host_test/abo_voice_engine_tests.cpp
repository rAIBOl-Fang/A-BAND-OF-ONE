#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "abo_p1/voice_engine.h"

namespace {

using abo_p1::Instrument;
using abo_p1::VoiceEngine;
using abo_p1::VoicePhase;

constexpr std::array<VoiceEngine::RootSample, 7> kPianoRoots{{
    {47, 120}, {54, 120}, {60, 120}, {66, 120},
    {72, 120}, {78, 120}, {84, 120},
}};

constexpr VoiceEngine::RootSample kFormalPianoC4Root{
    60, 125257U, 63360U, 125257U, nullptr,
};

void test_nearest_root_and_pitch_step() {
  VoiceEngine engine({2, 3});
  assert(engine.note_on(Instrument::Piano, 0, 4, kPianoRoots.data(),
                        kPianoRoots.size()));
  assert(engine.target_midi() == 60);
  assert(engine.source_root_midi() == 60);
  assert(engine.phase_step_q16() == 65536U);

  assert(engine.note_on(Instrument::Piano, 6, 5, kPianoRoots.data(),
                        kPianoRoots.size()));
  assert(engine.target_midi() == 83);
  assert(engine.source_root_midi() == 84);
  assert(std::abs(static_cast<double>(engine.phase_step_q16()) / 65536.0 -
                  std::pow(2.0, -1.0 / 12.0)) < 0.001);
}

void test_short_and_long_note_lifecycle() {
  VoiceEngine engine({2, 3});
  assert(engine.note_on(Instrument::Piano, 0, 4, kPianoRoots.data(),
                        kPianoRoots.size()));
  assert(engine.phase() == VoicePhase::Attack);
  engine.advance(2);
  assert(engine.phase() == VoicePhase::Sustain);
  engine.note_off();
  assert(engine.phase() == VoicePhase::Release);
  engine.advance(3);
  assert(engine.phase() == VoicePhase::Idle);

  assert(engine.note_on(Instrument::Piano, 1, 4, kPianoRoots.data(),
                        kPianoRoots.size()));
  engine.note_off();
  assert(engine.phase() == VoicePhase::Release);
  engine.advance(3);
  assert(engine.phase() == VoicePhase::Idle);
}

void test_sustain_loop_wraps_without_gap() {
  VoiceEngine engine({1, 2});
  assert(engine.note_on(Instrument::Piano, 0, 4, kPianoRoots.data(),
                        kPianoRoots.size()));
  assert(engine.set_source_cursor(119U << 16U));
  engine.advance_source(1U << 16U);
  assert(engine.source_cursor_q16() == 0U);
  engine.note_off();
}

void test_q16_cursor_reaches_and_wraps_formal_piano_loop() {
  VoiceEngine engine({1, 2});
  assert(engine.note_on(Instrument::Piano, 0, 4,
                        &kFormalPianoC4Root, 1U));

  constexpr std::uint64_t kOneSampleQ16 = 1ULL << 16U;
  assert(engine.set_source_cursor(65535ULL * kOneSampleQ16));
  engine.advance_source(static_cast<std::uint32_t>(kOneSampleQ16));
  assert(engine.source_cursor_q16() == 65536ULL * kOneSampleQ16);

  assert(engine.set_source_cursor(125256ULL * kOneSampleQ16));
  engine.advance_source(static_cast<std::uint32_t>(kOneSampleQ16));
  assert(engine.source_cursor_q16() == 63360ULL * kOneSampleQ16);
}

}  // namespace

int main() {
  test_nearest_root_and_pitch_step();
  test_short_and_long_note_lifecycle();
  test_sustain_loop_wraps_without_gap();
  test_q16_cursor_reaches_and_wraps_formal_piano_loop();
  return 0;
}
