#include "abo_p2/performance_clock.h"

namespace abo_p2 {

PerformanceClock::PerformanceClock(std::uint16_t bpm) { reset(bpm); }

void PerformanceClock::reset(std::uint16_t bpm) {
  bpm_ = bpm == 0U ? 90U : bpm;
  total_ticks_ = 0U;
  remainder_ = 0U;
}

std::uint64_t PerformanceClock::advance(std::uint32_t frames) {
  const auto denominator = static_cast<std::uint64_t>(kSampleRateHz) *
                           kSecondsPerMinute;
  const auto numerator = static_cast<std::uint64_t>(frames) * bpm_ *
                             kTicksPerQuarter +
                         remainder_;
  const auto emitted = numerator / denominator;
  remainder_ = numerator % denominator;
  total_ticks_ += emitted;
  return emitted;
}

}  // namespace abo_p2
