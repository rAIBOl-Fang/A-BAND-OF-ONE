#include <cassert>
#include <cstdint>
#include <type_traits>

#include "abo_p1/voice_command_queue.h"

int main() {
  using abo_p1::VoiceCommand;
  using abo_p1::VoiceCommandQueue;
  using abo_p1::VoiceCommandType;

  static_assert(VoiceCommandQueue::kCapacity == 16U);
  static_assert(std::is_trivially_copyable<VoiceCommand>::value);

  VoiceCommandQueue queue;
  for (std::size_t index = 0U; index < VoiceCommandQueue::kCapacity; ++index) {
    const VoiceCommand command{
        VoiceCommandType::NoteOn,
        static_cast<std::uint8_t>(index),
        static_cast<std::uint8_t>(index % 7U),
        4,
        static_cast<std::uint32_t>(100U + index),
    };
    assert(queue.push(command));
  }
  assert(queue.size() == VoiceCommandQueue::kCapacity);

  assert(!queue.push(VoiceCommand{
      VoiceCommandType::NoteOff, 3U, 0U, 4, 999U}));
  assert(queue.dropped_count() == 1U);

  for (std::size_t index = 0U; index < VoiceCommandQueue::kCapacity; ++index) {
    VoiceCommand command{};
    assert(queue.pop(&command));
    assert(command.type == VoiceCommandType::NoteOn);
    assert(command.key_index == index);
    assert(command.generation == 100U + index);
  }
  assert(queue.empty());
  VoiceCommand command{};
  assert(!queue.pop(&command));

  queue.reset_dropped_count();
  assert(queue.dropped_count() == 0U);
  assert(queue.push(VoiceCommand{
      VoiceCommandType::NoteOff, 3U, 0U, 4, 1234U}));
  queue.clear();
  assert(queue.empty());
  assert(!queue.pop(&command));
  return 0;
}
