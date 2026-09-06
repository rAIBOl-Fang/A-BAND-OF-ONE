#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "abo_p1/voice_renderer.h"

namespace {

using abo_p1::Instrument;
using abo_p1::VoiceEngine;
using abo_p1::VoicePhase;
using abo_p1::VoiceRenderer;

constexpr std::array<std::int16_t, 8> kSource{{
    1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000,
}};
constexpr std::array<VoiceEngine::RootSample, 1> kRoots{{
    {60, static_cast<std::uint32_t>(kSource.size()), 2, 6, kSource.data()},
}};

void test_render_uses_linear_interpolation() {
  VoiceRenderer renderer({1, 2});
  assert(renderer.note_on(Instrument::Piano, 0, 4,
                          kRoots.data(), kRoots.size()));

  std::array<std::int16_t, 2> output{};
  assert(renderer.render(output.data(), output.size()) == output.size());
  assert(output[0] > 0);
  assert(output[1] > output[0]);
}

void test_sustain_loop_has_no_silent_gap() {
  VoiceRenderer renderer({1, 2});
  assert(renderer.note_on(Instrument::Piano, 0, 4,
                          kRoots.data(), kRoots.size()));

  std::array<std::int16_t, 16> output{};
  assert(renderer.render(output.data(), output.size()) == output.size());
  for (std::size_t index = 1; index < output.size(); ++index) {
    assert(output[index] != 0);
  }
  assert(renderer.phase() == VoicePhase::Sustain);
}

void test_note_off_fades_to_idle_and_then_silence() {
  VoiceRenderer renderer({1, 2});
  assert(renderer.note_on(Instrument::Piano, 0, 4,
                          kRoots.data(), kRoots.size()));
  std::array<std::int16_t, 4> output{};
  renderer.render(output.data(), output.size());

  renderer.note_off();
  assert(renderer.phase() == VoicePhase::Release);
  assert(renderer.render(output.data(), output.size()) == output.size());
  assert(renderer.phase() == VoicePhase::Idle);
  assert(output[0] != 0);
  assert(output[1] != 0);
  assert(output[2] == 0);
  assert(output[3] == 0);
}

void test_clarinet_long_note_crosses_two_loops_without_gap() {
  constexpr std::size_t kSampleCount = 38400U;
  constexpr std::size_t kLoopStart = 20160U;
  constexpr std::size_t kLoopEnd = 29472U;
  constexpr std::size_t kLoopLength = kLoopEnd - kLoopStart;
  std::vector<std::int16_t> source(kSampleCount);
  for (std::size_t index = 0U; index < source.size(); ++index) {
    source[index] = static_cast<std::int16_t>(1000 + (index % 20000U));
  }
  const VoiceEngine::RootSample root{
      65, static_cast<std::uint32_t>(source.size()),
      static_cast<std::uint32_t>(kLoopStart),
      static_cast<std::uint32_t>(kLoopEnd), source.data()};

  VoiceRenderer renderer({1U, 96U});
  assert(renderer.note_on(Instrument::Clarinet, 3U, 4,
                          &root, 1U));
  const std::size_t render_frames = kLoopEnd + 2U * kLoopLength + 10U;
  std::vector<std::int16_t> output(render_frames);
  assert(renderer.render(output.data(), output.size()) == output.size());
  assert(renderer.phase() == VoicePhase::Sustain);
  for (const auto sample : output) assert(sample != 0);
}

void test_clarinet_short_note_enters_release() {
  constexpr std::array<std::int16_t, 4> kSource{{1000, 1200, 1400, 1600}};
  const VoiceEngine::RootSample root{
      65, 4U, 2U, 4U, kSource.data()};
  VoiceRenderer renderer({1U, 4U});
  assert(renderer.note_on(Instrument::Clarinet, 3U, 4,
                          &root, 1U));
  std::array<std::int16_t, 1> attack{};
  renderer.render(attack.data(), attack.size());
  renderer.note_off();
  assert(renderer.phase() == VoicePhase::Release);
  std::array<std::int16_t, 4> release{};
  assert(renderer.render(release.data(), release.size()) == release.size());
  assert(renderer.phase() == VoicePhase::Idle);
  assert(release[0] != 0);
  assert(release[3] != 0);
}

}  // namespace

int main() {
  test_render_uses_linear_interpolation();
  test_sustain_loop_has_no_silent_gap();
  test_note_off_fades_to_idle_and_then_silence();
  test_clarinet_long_note_crosses_two_loops_without_gap();
  test_clarinet_short_note_enters_release();
  return 0;
}
