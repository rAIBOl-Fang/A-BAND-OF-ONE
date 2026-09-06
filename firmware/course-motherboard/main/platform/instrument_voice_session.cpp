#include "platform/instrument_voice_session.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "speaker_assets/sound_asset_reader.h"

namespace easy_input {
namespace {

constexpr const char* kTag = "instrument_voice";

void log_bank_metrics(const char* event,
                      const speaker_assets::AboP1EncodedBank& bank,
                      int64_t elapsed_us,
                      esp_err_t result) {
  ESP_LOGW(kTag,
           "P1_DIAG %s instrument=%u result=%d bank_load_ms=%u arena_bytes=%u "
           "free_psram=%u largest_psram=%u",
           event, static_cast<unsigned>(bank.instrument_index),
           static_cast<int>(result),
           static_cast<unsigned>(elapsed_us / 1000LL),
           static_cast<unsigned>(InstrumentVoiceSession::kMaxPcmSamples *
                                sizeof(std::int16_t)),
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           static_cast<unsigned>(
               heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

esp_err_t decoder_error(speaker_assets::SoundAssetReadResult result) {
  switch (result) {
    case speaker_assets::SoundAssetReadResult::InvalidArgument:
      return ESP_ERR_INVALID_ARG;
    case speaker_assets::SoundAssetReadResult::OutputTooSmall:
      return ESP_ERR_INVALID_SIZE;
    case speaker_assets::SoundAssetReadResult::End:
      return ESP_ERR_INVALID_RESPONSE;
    case speaker_assets::SoundAssetReadResult::Ok:
      return ESP_OK;
    default:
      return ESP_ERR_INVALID_RESPONSE;
  }
}

}  // namespace

InstrumentVoiceSession::InstrumentVoiceSession()
    : mixer_({1U, 5760U, 480U}) {}

InstrumentVoiceSession::~InstrumentVoiceSession() {
  if (arena_ != nullptr) {
    heap_caps_free(arena_);
  }
}

esp_err_t InstrumentVoiceSession::begin(
    const speaker_assets::AboP1EncodedBank& bank) {
  if (ready_) return ESP_OK;
  if (bank.roots == nullptr || bank.root_count == 0U ||
      bank.root_count > kMaxRoots ||
      bank.decoded_samples_total > kMaxPcmSamples) {
    return ESP_ERR_INVALID_ARG;
  }

  arena_ = static_cast<std::int16_t*>(heap_caps_malloc(
      kMaxPcmSamples * sizeof(std::int16_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (arena_ == nullptr) return ESP_ERR_NO_MEM;
  arena_samples_ = kMaxPcmSamples;

  const auto started_us = esp_timer_get_time();
  const auto result = load_bank(bank);
  log_bank_metrics("bank_begin", bank, esp_timer_get_time() - started_us,
                   result);
  if (result != ESP_OK) {
    ESP_LOGE(kTag, "bank_load_failed instrument=%u error=%d",
             static_cast<unsigned>(bank.instrument_index),
             static_cast<int>(result));
    heap_caps_free(arena_);
    arena_ = nullptr;
    arena_samples_ = 0U;
    return result;
  }
  current_bank_ = &bank;
  ready_ = true;
  return ESP_OK;
}

bool InstrumentVoiceSession::request_bank(
    const speaker_assets::AboP1EncodedBank* bank) {
  if (!ready_ || bank == nullptr || bank->roots == nullptr ||
      bank->root_count == 0U || bank->root_count > kMaxRoots ||
      bank == current_bank_ || pending_bank_ != nullptr ||
      bank_swap_requested_.load(std::memory_order_acquire) ||
      bank_loading_.load(std::memory_order_acquire)) {
    return false;
  }
  pending_bank_ = bank;
  bank_swap_requested_.store(true, std::memory_order_release);
  return true;
}

bool InstrumentVoiceSession::poll_bank_swap() {
  if (!ready_ || pending_bank_ == nullptr ||
      !bank_safe_.load(std::memory_order_acquire)) {
    return false;
  }

  const auto* next_bank = pending_bank_;
  const auto* previous_bank = current_bank_;
  bank_loading_.store(true, std::memory_order_release);
  const auto started_us = esp_timer_get_time();
  const auto result = load_bank(*next_bank);
  if (result != ESP_OK) {
    ESP_LOGE(kTag, "bank_load_failed instrument=%u error=%d",
             static_cast<unsigned>(next_bank->instrument_index),
             static_cast<int>(result));
    const auto restore_result =
        previous_bank == nullptr ? ESP_ERR_INVALID_STATE
                                 : load_bank(*previous_bank);
    if (restore_result != ESP_OK) {
      ESP_LOGE(kTag, "bank_restore_failed error=%d",
               static_cast<int>(restore_result));
      ready_ = false;
    }
    last_bank_error_.store(static_cast<std::int32_t>(result),
                           std::memory_order_release);
  } else {
    current_bank_ = next_bank;
    last_bank_error_.store(ESP_OK, std::memory_order_release);
  }

  log_bank_metrics("bank_swap", *next_bank,
                   esp_timer_get_time() - started_us, result);

  pending_bank_ = nullptr;
  bank_loading_.store(false, std::memory_order_release);
  bank_resume_.store(true, std::memory_order_release);
  return result == ESP_OK;
}

std::uint8_t InstrumentVoiceSession::current_instrument_index() const {
  return current_bank_ == nullptr ? 0U : current_bank_->instrument_index;
}

bool InstrumentVoiceSession::note_on(
    std::uint8_t key_index,
    std::uint32_t generation,
    abo_p1::Instrument instrument,
    std::uint8_t solfege_index,
    std::int8_t octave) {
  if (!ready_ || root_count_ == 0U || bank_loading() ||
      static_cast<std::uint8_t>(instrument) != current_instrument_index()) {
    return false;
  }
  return command_queue_.push({abo_p1::VoiceCommandType::NoteOn,
                              key_index,
                              solfege_index,
                              octave,
                              generation});
}

bool InstrumentVoiceSession::note_off(std::uint8_t key_index,
                                      std::uint32_t generation) {
  if (!ready_) return false;
  return command_queue_.push({abo_p1::VoiceCommandType::NoteOff,
                              key_index,
                              0U,
                              0,
                              generation});
}

bool InstrumentVoiceSession::set_volume(std::uint8_t volume) noexcept {
  const auto normalized = volume > 100U ? 100U : volume;
  const auto current =
      target_volume_mailbox_.load(std::memory_order_relaxed);
  const auto next_generation = (current >> 8U) + 1U;
  target_volume_mailbox_.store(
      pack_volume_target(next_generation, normalized),
      std::memory_order_release);
  return true;
}

std::uint8_t InstrumentVoiceSession::target_volume() const noexcept {
  return static_cast<std::uint8_t>(target_volume_mailbox_.load(
      std::memory_order_acquire) & kVolumeMask);
}

bool InstrumentVoiceSession::render_frame(std::int16_t* output,
                                          std::size_t output_samples,
                                          bool* finished) {
  if (output == nullptr || finished == nullptr || output_samples == 0U) {
    return false;
  }
  *finished = false;

  // Only the audio consumer mutates mixer/renderer state. The main task can
  // overwrite the single PSRAM arena only after this boundary is published.
  if (bank_swap_requested_.exchange(false, std::memory_order_acq_rel)) {
    mixer_.reset();
    apply_pending_volume(true);
    command_queue_.clear();
    audio_waiting_for_bank_ = true;
    bank_safe_.store(true, std::memory_order_release);
  }
  if (audio_waiting_for_bank_) {
    std::fill(output, output + output_samples, 0);
    if (bank_resume_.exchange(false, std::memory_order_acq_rel)) {
      audio_waiting_for_bank_ = false;
      bank_safe_.store(false, std::memory_order_release);
    }
    rendered_frames_.fetch_add(output_samples, std::memory_order_release);
    return true;
  }

  apply_pending_volume();
  apply_pending_commands();
  mixer_.render(output, output_samples);
  if (exit_requested_.load(std::memory_order_acquire) && !mixer_.active()) {
    std::fill(output, output + output_samples, 0);
    *finished = true;
  }
  rendered_frames_.fetch_add(output_samples, std::memory_order_release);
  return true;
}

void InstrumentVoiceSession::apply_pending_volume(bool force) noexcept {
  const auto mailbox =
      target_volume_mailbox_.load(std::memory_order_acquire);
  const auto generation = mailbox >> 8U;
  if (!force && generation == volume_generation_) return;

  mixer_.set_volume(static_cast<std::uint8_t>(mailbox & kVolumeMask));
  volume_generation_ = generation;
}

void InstrumentVoiceSession::apply_pending_commands() {
  abo_p1::VoiceCommand command;
  while (command_queue_.pop(&command)) {
    if (command.type == abo_p1::VoiceCommandType::NoteOn) {
      mixer_.note_on(command.key_index,
                     command.generation,
                     static_cast<abo_p1::Instrument>(current_instrument_index()),
                     command.solfege_index,
                     command.octave,
                     roots_.data(),
                     root_count_);
    } else if (command.type == abo_p1::VoiceCommandType::NoteOff) {
      mixer_.note_off(command.key_index, command.generation);
    }
  }
}

esp_err_t InstrumentVoiceSession::load_bank(
    const speaker_assets::AboP1EncodedBank& bank) {
  std::array<abo_p1::VoiceEngine::RootSample, kMaxRoots> decoded_roots{};
  std::size_t decoded_root_count = 0U;
  const auto result = decode_bank(bank, decoded_roots.data(),
                                  &decoded_root_count);
  if (result != ESP_OK) return result;
  roots_ = decoded_roots;
  root_count_ = decoded_root_count;
  arena_samples_ = bank.decoded_samples_total;
  return ESP_OK;
}

esp_err_t InstrumentVoiceSession::decode_bank(
    const speaker_assets::AboP1EncodedBank& bank,
    abo_p1::VoiceEngine::RootSample* roots,
    std::size_t* root_count) {
  if (arena_ == nullptr || roots == nullptr || root_count == nullptr ||
      bank.roots == nullptr || bank.root_count == 0U ||
      bank.root_count > kMaxRoots ||
      bank.decoded_samples_total > kMaxPcmSamples) {
    return ESP_ERR_INVALID_ARG;
  }

  std::array<std::int16_t, kFrameSamples> frame{};
  std::size_t arena_offset = 0U;
  std::size_t total_decoded = 0U;
  for (std::size_t index = 0U; index < bank.root_count; ++index) {
    const auto& encoded_root = bank.roots[index];
    if (encoded_root.payload == nullptr || encoded_root.payload_bytes == 0U ||
        encoded_root.decoded_samples == 0U ||
        encoded_root.loop_end_sample > encoded_root.decoded_samples ||
        encoded_root.loop_start_sample >= encoded_root.loop_end_sample) {
      return ESP_ERR_INVALID_ARG;
    }
    if (arena_offset + encoded_root.decoded_samples > kMaxPcmSamples) {
      return ESP_ERR_INVALID_SIZE;
    }

    std::size_t root_decoded = 0U;
    switch (encoded_root.storage) {
      case speaker_assets::AboP1RootStorage::EiadV1: {
        speaker_assets::SoundAssetStreamDecoder decoder;
        const auto open_result = decoder.open_embedded(
            encoded_root.payload, encoded_root.payload_bytes);
        if (open_result != speaker_assets::SoundAssetReadResult::Ok) {
          decoder.close();
          return decoder_error(open_result);
        }
        if (decoder.asset().decoded_samples != encoded_root.decoded_samples) {
          decoder.close();
          return ESP_ERR_INVALID_SIZE;
        }

        while (root_decoded < encoded_root.decoded_samples) {
          std::size_t decoded = 0U;
          const auto read_result = decoder.decode_next(
              frame.data(), frame.size(), &decoded);
          if (read_result != speaker_assets::SoundAssetReadResult::Ok ||
              decoded == 0U ||
              decoded > encoded_root.decoded_samples - root_decoded) {
            decoder.close();
            return decoder_error(read_result);
          }
          std::memcpy(arena_ + arena_offset + root_decoded, frame.data(),
                      decoded * sizeof(frame[0]));
          root_decoded += decoded;
        }
        decoder.close();
        break;
      }
      case speaker_assets::AboP1RootStorage::Pcm16Le: {
        if (encoded_root.decoded_samples >
            std::numeric_limits<std::size_t>::max() /
                sizeof(std::int16_t)) {
          return ESP_ERR_INVALID_SIZE;
        }
        const auto expected_bytes =
            static_cast<std::size_t>(encoded_root.decoded_samples) *
            sizeof(std::int16_t);
        if (encoded_root.payload_bytes != expected_bytes) {
          return ESP_ERR_INVALID_SIZE;
        }
        std::memcpy(arena_ + arena_offset, encoded_root.payload,
                    expected_bytes);
        root_decoded = encoded_root.decoded_samples;
        break;
      }
      default:
        return ESP_ERR_INVALID_ARG;
    }

    roots[index] = abo_p1::VoiceEngine::RootSample{
        static_cast<std::int16_t>(encoded_root.root_midi),
        encoded_root.decoded_samples,
        encoded_root.loop_start_sample,
        encoded_root.loop_end_sample,
        arena_ + arena_offset,
    };
    arena_offset += encoded_root.decoded_samples;
    total_decoded += encoded_root.decoded_samples;
  }

  if (total_decoded != bank.decoded_samples_total) {
    return ESP_ERR_INVALID_SIZE;
  }
  *root_count = bank.root_count;
  return ESP_OK;
}

bool instrument_voice_frame_provider(void* context,
                                     std::int16_t* output,
                                     std::size_t output_samples,
                                     bool* finished) {
  if (context == nullptr) return false;
  return static_cast<InstrumentVoiceSession*>(context)->render_frame(
      output, output_samples, finished);
}

}  // namespace easy_input
