#include <cassert>
#include <cstdint>
#include <initializer_list>

#include "abo_p2/performance_clock.h"

namespace {

using abo_p2::PerformanceClock;

constexpr std::uint64_t kSampleRate = 48000ULL;
constexpr std::uint64_t kTicksPerQuarter = 96ULL;
constexpr std::uint64_t kSecondsPerMinute = 60ULL;

std::uint64_t expected_numerator(std::uint32_t frames,
                                 std::uint16_t bpm) {
  return static_cast<std::uint64_t>(frames) * bpm * kTicksPerQuarter;
}

std::uint64_t expected_denominator(std::uint16_t bpm) {
  return kSampleRate * kSecondsPerMinute;
}

void one_quarter_at_bpm_60_is_96_ticks() {
  PerformanceClock clock(60U);
  assert(clock.advance(48000U) == 96U);
  assert(clock.total_ticks() == 96U);
  assert(clock.remainder() == 0U);
}

void common_bpm_values_preserve_fractional_ticks() {
  for (const auto bpm : {40U, 60U, 90U, 240U}) {
    PerformanceClock clock(static_cast<std::uint16_t>(bpm));
    const auto emitted = clock.advance(48000U);
    const auto numerator = expected_numerator(48000U,
                                               static_cast<std::uint16_t>(bpm));
    const auto denominator = expected_denominator(
        static_cast<std::uint16_t>(bpm));
    assert(clock.total_ticks() == numerator / denominator);
    assert(clock.remainder() == numerator % denominator);
    assert(emitted == clock.total_ticks());
  }
}

void chunk_size_does_not_change_ten_minute_result() {
  constexpr std::uint32_t total_frames = 48000U * 60U * 10U;
  for (const auto bpm : {40U, 60U, 90U, 240U}) {
    PerformanceClock by_128(static_cast<std::uint16_t>(bpm));
    PerformanceClock by_480(static_cast<std::uint16_t>(bpm));
    for (std::uint32_t frames = 0U; frames < total_frames; frames += 128U) {
      by_128.advance(frames + 128U > total_frames ? total_frames - frames
                                                   : 128U);
    }
    for (std::uint32_t frames = 0U; frames < total_frames; frames += 480U) {
      by_480.advance(frames + 480U > total_frames ? total_frames - frames
                                                   : 480U);
    }

    const auto numerator = expected_numerator(
        total_frames, static_cast<std::uint16_t>(bpm));
    const auto denominator = expected_denominator(
        static_cast<std::uint16_t>(bpm));
    assert(by_128.total_ticks() == numerator / denominator);
    assert(by_128.remainder() == numerator % denominator);
    assert(by_480.total_ticks() == by_128.total_ticks());
    assert(by_480.remainder() == by_128.remainder());
    assert(by_128.total_ticks() * denominator + by_128.remainder() ==
           numerator);
  }
}

void reset_discards_partial_tick_without_float_state() {
  PerformanceClock clock(90U);
  clock.advance(128U);
  assert(clock.total_ticks() == 0U);
  assert(clock.remainder() != 0U);
  clock.reset(60U);
  assert(clock.total_ticks() == 0U);
  assert(clock.remainder() == 0U);
  assert(clock.advance(48000U) == 96U);
}

}  // namespace

int main() {
  one_quarter_at_bpm_60_is_96_ticks();
  common_bpm_values_preserve_fractional_ticks();
  chunk_size_does_not_change_ten_minute_result();
  reset_discards_partial_tick_without_float_state();
  return 0;
}
