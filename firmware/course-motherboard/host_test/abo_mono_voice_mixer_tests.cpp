#include <array>
#include <cassert>
#include <cstdint>

#include "abo_p1/mono_voice_mixer.h"

namespace {

using abo_p1::Instrument;
using abo_p1::MonoVoiceMixer;
using abo_p1::VoiceEngine;

constexpr std::array<std::int16_t, 16U> kPositiveSamples{
    32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000,
    32000, 32000, 32000, 32000, 32000, 32000, 32000, 32000,
};
constexpr std::array<std::int16_t, 16U> kNegativeSamples{
    -32000, -32000, -32000, -32000, -32000, -32000, -32000, -32000,
    -32000, -32000, -32000, -32000, -32000, -32000, -32000, -32000,
};

constexpr VoiceEngine::RootSample kPositiveRoot{
    60, 16U, 2U, 14U, kPositiveSamples.data()};
constexpr VoiceEngine::RootSample kNegativeRoot{
    60, 16U, 2U, 14U, kNegativeSamples.data()};

bool note_on(MonoVoiceMixer* mixer,
             std::uint8_t key_index,
             std::uint32_t generation,
             const VoiceEngine::RootSample* root) {
  return mixer->note_on(
      key_index, generation, Instrument::Piano, 0U, 4, root, 1U);
}

void test_idle_is_silent_and_note_starts() {
  MonoVoiceMixer mixer(MonoVoiceMixer::Config{1U, 3U, 480U});
  std::array<std::int16_t, 8U> output{};
  assert(mixer.render(output.data(), output.size()) == output.size());
  for (const auto sample : output) {
    assert(sample == 0);
  }

  assert(note_on(&mixer, 1U, 1U, &kPositiveRoot));
  assert(mixer.active());
  assert(mixer.current_generation() == 1U);
  output.fill(0);
  mixer.render(output.data(), output.size());
  bool heard = false;
  for (const auto sample : output) {
    heard = heard || sample > 0;
  }
  assert(heard);
}

void test_retrigger_is_generation_safe_and_bounded() {
  MonoVoiceMixer mixer(MonoVoiceMixer::Config{1U, 3U, 480U});
  std::array<std::int16_t, 8U> warmup{};
  assert(note_on(&mixer, 1U, 1U, &kPositiveRoot));
  mixer.render(warmup.data(), warmup.size());

  assert(note_on(&mixer, 1U, 2U, &kNegativeRoot));
  assert(mixer.current_key_index() == 1U);
  assert(mixer.current_generation() == 2U);
  assert(mixer.crossfade_active());
  assert(!mixer.note_off(1U, 1U));

  std::array<std::int16_t, 480U> output{};
  mixer.render(output.data(), output.size());
  for (const auto sample : output) {
    assert(sample >= -32000);
    assert(sample <= 32000);
  }
  assert(!mixer.crossfade_active());
  assert(mixer.crossfade_frames_remaining() == 0U);

  assert(note_on(&mixer, 2U, 3U, &kPositiveRoot));
  assert(mixer.current_key_index() == 2U);
  assert(mixer.current_generation() == 3U);
  assert(mixer.active_voice_count() <= 2U);
}

void test_current_release_reaches_idle_without_gap() {
  MonoVoiceMixer mixer(MonoVoiceMixer::Config{1U, 3U, 480U});
  assert(note_on(&mixer, 4U, 7U, &kPositiveRoot));
  std::array<std::int16_t, 8U> output{};
  mixer.render(output.data(), output.size());
  assert(mixer.note_off(4U, 7U));

  output.fill(0);
  mixer.render(output.data(), 3U);
  assert(!mixer.active());
  output.fill(1);
  mixer.render(output.data(), output.size());
  for (const auto sample : output) {
    assert(sample == 0);
  }
}

void test_default_volume_is_bit_equivalent_and_zero_mutes() {
  MonoVoiceMixer baseline(MonoVoiceMixer::Config{1U, 3U, 480U});
  MonoVoiceMixer explicit_seventy(MonoVoiceMixer::Config{1U, 3U, 480U});
  assert(note_on(&baseline, 1U, 1U, &kPositiveRoot));
  assert(note_on(&explicit_seventy, 1U, 1U, &kPositiveRoot));
  explicit_seventy.set_volume(70U);

  std::array<std::int16_t, 16U> baseline_output{};
  std::array<std::int16_t, 16U> explicit_output{};
  baseline.render(baseline_output.data(), baseline_output.size());
  explicit_seventy.render(explicit_output.data(), explicit_output.size());
  assert(baseline_output == explicit_output);

  explicit_seventy.set_volume(0U);
  std::array<std::int16_t, 480U> ramp{};
  explicit_seventy.render(ramp.data(), ramp.size());
  assert(ramp.back() == 0);
  std::array<std::int16_t, 16U> silence{};
  explicit_seventy.render(silence.data(), silence.size());
  for (const auto sample : silence) assert(sample == 0);
}

void test_max_volume_saturates_symmetrically() {
  MonoVoiceMixer positive(MonoVoiceMixer::Config{1U, 3U, 480U});
  MonoVoiceMixer negative(MonoVoiceMixer::Config{1U, 3U, 480U});
  assert(note_on(&positive, 1U, 1U, &kPositiveRoot));
  assert(note_on(&negative, 1U, 1U, &kNegativeRoot));

  std::array<std::int16_t, 8U> warmup{};
  positive.render(warmup.data(), warmup.size());
  negative.render(warmup.data(), warmup.size());
  positive.set_volume(100U);
  negative.set_volume(100U);
  std::array<std::int16_t, 480U> positive_output{};
  std::array<std::int16_t, 480U> negative_output{};
  positive.render(positive_output.data(), positive_output.size());
  negative.render(negative_output.data(), negative_output.size());
  assert(positive_output.back() == 32767);
  assert(negative_output.back() == -32768);
}

void test_volume_ramp_is_monotonic_across_buffers_and_crossfade() {
  MonoVoiceMixer mixer(MonoVoiceMixer::Config{1U, 3U, 480U});
  assert(note_on(&mixer, 1U, 1U, &kPositiveRoot));
  std::array<std::int16_t, 8U> warmup{};
  mixer.render(warmup.data(), warmup.size());

  mixer.set_volume(0U);
  std::array<std::int16_t, 240U> first{};
  std::array<std::int16_t, 240U> second{};
  mixer.render(first.data(), first.size());
  mixer.render(second.data(), second.size());
  for (std::size_t index = 1U; index < first.size(); ++index) {
    assert(first[index] <= first[index - 1U]);
  }
  assert(second.front() <= first.back());
  for (std::size_t index = 1U; index < second.size(); ++index) {
    assert(second[index] <= second[index - 1U]);
  }

  assert(note_on(&mixer, 2U, 2U, &kNegativeRoot));
  mixer.set_volume(100U);
  std::array<std::int16_t, 480U> crossfade{};
  mixer.render(crossfade.data(), crossfade.size());
  for (std::size_t index = 1U; index < crossfade.size(); ++index) {
    const auto delta = static_cast<std::int32_t>(crossfade[index]) -
                       static_cast<std::int32_t>(crossfade[index - 1U]);
    assert(delta < 4000);
    assert(delta > -4000);
  }
}

void test_rapid_volume_updates_follow_latest_target() {
  MonoVoiceMixer mixer(MonoVoiceMixer::Config{1U, 3U, 480U});
  assert(note_on(&mixer, 1U, 1U, &kPositiveRoot));
  std::array<std::int16_t, 8U> warmup{};
  mixer.render(warmup.data(), warmup.size());

  mixer.set_volume(10U);
  mixer.set_volume(90U);
  mixer.set_volume(20U);
  assert(mixer.target_volume() == 20U);
  std::array<std::int16_t, 480U> output{};
  mixer.render(output.data(), output.size());
  assert(!mixer.volume_ramp_active());
  assert(mixer.volume() == 20U);
  assert(output.back() > 8000);
  assert(output.back() < 10000);
}

}  // namespace

int main() {
  test_idle_is_silent_and_note_starts();
  test_retrigger_is_generation_safe_and_bounded();
  test_current_release_reaches_idle_without_gap();
  test_default_volume_is_bit_equivalent_and_zero_mutes();
  test_max_volume_saturates_symmetrically();
  test_volume_ramp_is_monotonic_across_buffers_and_crossfade();
  test_rapid_volume_updates_follow_latest_target();
  return 0;
}
