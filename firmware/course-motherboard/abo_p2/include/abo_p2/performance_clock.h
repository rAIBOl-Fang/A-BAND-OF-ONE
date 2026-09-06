#pragma once

#include <cstdint>

namespace abo_p2 {

class PerformanceClock {
 public:
  static constexpr std::uint32_t kSampleRateHz = 48000U;
  static constexpr std::uint16_t kTicksPerQuarter = 96U;

  explicit PerformanceClock(std::uint16_t bpm = 90U);

  void reset(std::uint16_t bpm);
  std::uint64_t advance(std::uint32_t frames);

  std::uint16_t bpm() const { return bpm_; }
  std::uint64_t total_ticks() const { return total_ticks_; }
  std::uint64_t remainder() const { return remainder_; }

 private:
  static constexpr std::uint64_t kSecondsPerMinute = 60ULL;

  std::uint16_t bpm_ = 90U;
  std::uint64_t total_ticks_ = 0U;
  std::uint64_t remainder_ = 0U;
};

}  // namespace abo_p2
