#include "platform/piano_voice_session.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "esp_heap_caps.h"
#include "speaker_assets/sound_asset_reader.h"

namespace easy_input {
namespace {

constexpr std::size_t kDecodeFrameSamples = 480U;
constexpr std::uint32_t kPianoMidiC4 = 60U;
constexpr std::uint32_t kPianoReleaseSamples = 5760U;

}  // namespace

PianoVoiceSession::PianoVoiceSession()
    : renderer_({1U, kPianoReleaseSamples}) {}

PianoVoiceSession::~PianoVoiceSession() {
  if (pcm_ != nullptr) {
    heap_caps_free(pcm_);
  }
}

esp_err_t PianoVoiceSession::begin(
    const speaker_assets::AboP1PianoSound& sound) {
  if (ready_) return ESP_OK;
  if (sound.encoded == nullptr || sound.encoded_bytes == 0U) {
    return ESP_ERR_INVALID_ARG;
  }

  speaker_assets::SoundAssetStreamDecoder decoder;
  if (decoder.open_embedded(sound.encoded, sound.encoded_bytes) !=
      speaker_assets::SoundAssetReadResult::Ok) {
    return ESP_ERR_INVALID_RESPONSE;
  }
  const auto decoded_samples = decoder.asset().decoded_samples;
  if (decoded_samples == 0U) {
    decoder.close();
    return ESP_ERR_INVALID_SIZE;
  }
  pcm_ = static_cast<std::int16_t*>(heap_caps_malloc(
      static_cast<std::size_t>(decoded_samples) * sizeof(std::int16_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (pcm_ == nullptr) {
    decoder.close();
    return ESP_ERR_NO_MEM;
  }

  std::array<std::int16_t, kDecodeFrameSamples> frame{};
  std::uint32_t offset = 0U;
  while (offset < decoded_samples) {
    std::size_t decoded = 0U;
    const auto result = decoder.decode_next(
        frame.data(), frame.size(), &decoded);
    if (result != speaker_assets::SoundAssetReadResult::Ok ||
        decoded == 0U ||
        decoded > decoded_samples - offset) {
      decoder.close();
      heap_caps_free(pcm_);
      pcm_ = nullptr;
      return ESP_ERR_INVALID_RESPONSE;
    }
    std::memcpy(pcm_ + offset, frame.data(),
                decoded * sizeof(std::int16_t));
    offset += static_cast<std::uint32_t>(decoded);
  }
  decoder.close();

  sample_count_ = decoded_samples;
  const auto loop_start_sample =
      static_cast<std::uint32_t>(sound.loop_start_frame) *
      speaker_assets::kSoundAssetFrameSamples;
  const auto requested_loop_end_sample =
      static_cast<std::uint32_t>(sound.loop_end_frame) *
      speaker_assets::kSoundAssetFrameSamples;
  const auto loop_end_sample =
      std::min(requested_loop_end_sample, decoded_samples);
  root_ = abo_p1::VoiceEngine::RootSample{
      static_cast<std::int16_t>(kPianoMidiC4),
      sample_count_,
      loop_start_sample,
      loop_end_sample,
      pcm_,
  };
  if (root_.loop_end_frame == 0U ||
      root_.loop_end_frame > sample_count_ ||
      root_.loop_start_frame >= root_.loop_end_frame) {
    heap_caps_free(pcm_);
    pcm_ = nullptr;
    return ESP_ERR_INVALID_ARG;
  }
  ready_ = true;
  return ESP_OK;
}

bool PianoVoiceSession::note_on(std::uint8_t key_index,
                                std::int8_t octave) {
  if (!ready_ || key_index != 0U) return false;
  if (renderer_.active()) return false;
  note_off_requested_.store(false, std::memory_order_release);
  return renderer_.note_on(abo_p1::Instrument::Piano,
                           key_index, octave, &root_, 1U);
}

void PianoVoiceSession::note_off() {
  note_off_requested_.store(true, std::memory_order_release);
}

bool PianoVoiceSession::render_frame(std::int16_t* output,
                                     std::size_t output_samples,
                                     bool* finished) {
  if (!ready_ || output == nullptr || finished == nullptr ||
      output_samples == 0U) {
    return false;
  }
  if (note_off_requested_.exchange(false, std::memory_order_acq_rel)) {
    renderer_.note_off();
  }
  if (renderer_.render(output, output_samples) != output_samples) {
    return false;
  }
  *finished = !renderer_.active();
  return true;
}

bool piano_voice_frame_provider(void* context,
                               std::int16_t* output,
                               std::size_t output_samples,
                               bool* finished) {
  if (context == nullptr) return false;
  return static_cast<PianoVoiceSession*>(context)->render_frame(
      output, output_samples, finished);
}

}  // namespace easy_input
