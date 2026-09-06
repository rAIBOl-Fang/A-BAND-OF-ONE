#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace abo_p1 {

enum class VoiceCommandType : std::uint8_t {
  NoteOn,
  NoteOff,
};

struct VoiceCommand {
  VoiceCommandType type = VoiceCommandType::NoteOff;
  std::uint8_t key_index = 0U;
  std::uint8_t solfege_index = 0U;
  std::int8_t octave = 0;
  std::uint32_t generation = 0U;
};

// Single-producer/single-consumer queue for the main/input task and the
// audio task. It is intentionally fixed-capacity so the audio path never
// allocates or waits for a lock.
class VoiceCommandQueue {
 public:
  static constexpr std::size_t kCapacity = 16U;

  bool push(const VoiceCommand& command);
  bool pop(VoiceCommand* command);

  void clear();
  bool empty() const;
  std::size_t size() const;
  std::uint32_t dropped_count() const;
  void reset_dropped_count();

 private:
  std::array<VoiceCommand, kCapacity> buffer_{};
  std::atomic<std::uint32_t> head_{0U};
  std::atomic<std::uint32_t> tail_{0U};
  std::atomic<std::uint32_t> dropped_{0U};
};

}  // namespace abo_p1
