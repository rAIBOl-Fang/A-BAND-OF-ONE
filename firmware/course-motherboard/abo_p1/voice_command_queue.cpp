#include "abo_p1/voice_command_queue.h"

#include <algorithm>

namespace abo_p1 {

bool VoiceCommandQueue::push(const VoiceCommand& command) {
  const auto tail = tail_.load(std::memory_order_relaxed);
  const auto head = head_.load(std::memory_order_acquire);
  if (tail - head >= kCapacity) {
    dropped_.fetch_add(1U, std::memory_order_relaxed);
    return false;
  }

  buffer_[tail % kCapacity] = command;
  tail_.store(tail + 1U, std::memory_order_release);
  return true;
}

bool VoiceCommandQueue::pop(VoiceCommand* command) {
  if (command == nullptr) return false;

  const auto head = head_.load(std::memory_order_relaxed);
  const auto tail = tail_.load(std::memory_order_acquire);
  if (head == tail) return false;

  *command = buffer_[head % kCapacity];
  head_.store(head + 1U, std::memory_order_release);
  return true;
}

void VoiceCommandQueue::clear() {
  const auto tail = tail_.load(std::memory_order_acquire);
  head_.store(tail, std::memory_order_release);
}

bool VoiceCommandQueue::empty() const {
  return size() == 0U;
}

std::size_t VoiceCommandQueue::size() const {
  const auto head = head_.load(std::memory_order_acquire);
  const auto tail = tail_.load(std::memory_order_acquire);
  return std::min<std::uint32_t>(tail - head,
                                 static_cast<std::uint32_t>(kCapacity));
}

std::uint32_t VoiceCommandQueue::dropped_count() const {
  return dropped_.load(std::memory_order_relaxed);
}

void VoiceCommandQueue::reset_dropped_count() {
  dropped_.store(0U, std::memory_order_relaxed);
}

}  // namespace abo_p1
