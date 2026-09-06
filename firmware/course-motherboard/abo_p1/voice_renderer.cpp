#include "abo_p1/voice_renderer.h"

#include <algorithm>
#include <cstdint>

namespace abo_p1 {
namespace {

constexpr std::int32_t kQ15One = 32767;

}  // namespace

VoiceRenderer::VoiceRenderer()
    : voice_() {}

VoiceRenderer::VoiceRenderer(Config config)
    : voice_({config.attack_frames, config.release_frames}) {}

void VoiceRenderer::reset() noexcept {
  selected_root_ = nullptr;
  voice_.reset();
}

bool VoiceRenderer::note_on(Instrument instrument,
                            std::uint8_t solfege_index,
                            std::int8_t octave,
                            const RootSample* roots,
                            std::size_t root_count) {
  if (roots == nullptr || root_count == 0U) return false;
  if (!voice_.note_on(instrument, solfege_index, octave,
                      roots, root_count)) {
    return false;
  }
  selected_root_ = find_selected_root(roots, root_count);
  if (selected_root_ == nullptr || selected_root_->samples == nullptr) {
    selected_root_ = nullptr;
    voice_.note_off();
    return false;
  }
  return true;
}

void VoiceRenderer::note_off() {
  voice_.note_off();
}

std::size_t VoiceRenderer::render(std::int16_t* output,
                                  std::size_t frame_count) {
  if (output == nullptr) return 0U;
  for (std::size_t index = 0U; index < frame_count; ++index) {
    if (!active() || selected_root_ == nullptr) {
      output[index] = 0;
      continue;
    }
    const auto sample = static_cast<std::int32_t>(sample_at_cursor());
    const auto gain = envelope_q15();
    output[index] = static_cast<std::int16_t>(
        std::clamp((sample * gain) / kQ15One,
                   static_cast<std::int32_t>(-32768),
                   static_cast<std::int32_t>(32767)));
    voice_.advance_source(voice_.phase_step_q16());
    voice_.advance(1U);
  }
  return frame_count;
}

const VoiceRenderer::RootSample* VoiceRenderer::find_selected_root(
    const RootSample* roots,
    std::size_t root_count) const {
  for (std::size_t index = 0U; index < root_count; ++index) {
    if (roots[index].midi == voice_.source_root_midi()) {
      return &roots[index];
    }
  }
  return nullptr;
}

std::int16_t VoiceRenderer::sample_at_cursor() const {
  const auto cursor = voice_.source_cursor_q16();
  const auto sample_count = selected_root_->frame_count;
  if (sample_count == 0U) return 0;
  const auto index = std::min<std::uint32_t>(cursor >> 16U,
                                             sample_count - 1U);
  auto next_index = index + 1U;
  const auto loop_start = selected_root_->loop_start_frame;
  const auto loop_end = selected_root_->loop_end_frame == 0U
                            ? sample_count
                            : selected_root_->loop_end_frame;
  if (next_index >= loop_end && index >= loop_start) {
    next_index = loop_start;
  } else if (next_index >= sample_count) {
    next_index = sample_count - 1U;
  }
  const auto first = static_cast<std::int32_t>(
      selected_root_->samples[index]);
  const auto second = static_cast<std::int32_t>(
      selected_root_->samples[next_index]);
  const auto fraction = static_cast<std::int32_t>(cursor & 0xFFFFU);
  return static_cast<std::int16_t>(
      first + ((second - first) * fraction) / 65536);
}

std::int32_t VoiceRenderer::envelope_q15() const {
  switch (voice_.phase()) {
    case VoicePhase::Attack:
      return std::clamp<std::int32_t>(
          static_cast<std::int32_t>(
              (voice_.phase_frames() + 1U) * kQ15One /
              std::max<std::uint32_t>(1U, voice_.attack_frames())),
          std::int32_t{0}, kQ15One);
    case VoicePhase::Release:
      return std::clamp<std::int32_t>(
          static_cast<std::int32_t>(
              (voice_.release_frames() -
               std::min(voice_.phase_frames(), voice_.release_frames())) *
              kQ15One /
              std::max<std::uint32_t>(1U, voice_.release_frames())),
          std::int32_t{0}, kQ15One);
    case VoicePhase::Sustain:
      return kQ15One;
    case VoicePhase::Idle:
      return 0;
  }
  return 0;
}

}  // namespace abo_p1
