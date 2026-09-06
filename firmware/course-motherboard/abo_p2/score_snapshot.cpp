#include "abo_p2/score_snapshot.h"

#include <algorithm>

namespace abo_p2 {

bool ScoreSnapshot::assign(std::string_view id,
                           std::uint16_t bpm,
                           const ScoreNote* notes,
                           std::size_t note_count) {
  if (id.empty() || id.size() > kMaxIdBytes || bpm == 0U || notes == nullptr ||
      note_count == 0U || note_count > kMaxNotes) {
    valid_ = false;
    return false;
  }

  std::fill(id_.begin(), id_.end(), '\0');
  std::copy(id.begin(), id.end(), id_.begin());
  id_length_ = static_cast<std::uint8_t>(id.size());
  std::copy_n(notes, note_count, notes_.begin());
  note_count_ = note_count;
  bpm_ = bpm;
  valid_ = true;
  return true;
}

}  // namespace abo_p2
