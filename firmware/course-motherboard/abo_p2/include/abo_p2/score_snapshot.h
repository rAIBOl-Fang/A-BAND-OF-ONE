#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "abo_p2/performance_types.h"

namespace abo_p2 {

// A committed score is copied into this fixed-capacity object before the
// controller can observe it. This prevents a later score_commit from
// mutating the score currently used by the running performance.
class ScoreSnapshot {
 public:
  static constexpr std::size_t kMaxIdBytes = 32U;
  static constexpr std::size_t kMaxNotes = 128U;

  bool assign(std::string_view id,
              std::uint16_t bpm,
              const ScoreNote* notes,
              std::size_t note_count);

  bool valid() const { return valid_; }
  std::string_view id() const {
    return std::string_view(id_.data(), id_length_);
  }
  ScoreView view() const {
    return ScoreView{id_.data(), bpm_, notes_.data(), note_count_};
  }

 private:
  std::array<char, kMaxIdBytes + 1U> id_{};
  std::array<ScoreNote, kMaxNotes> notes_{};
  std::uint8_t id_length_ = 0U;
  std::uint16_t bpm_ = 90U;
  std::size_t note_count_ = 0U;
  bool valid_ = false;
};

}  // namespace abo_p2
