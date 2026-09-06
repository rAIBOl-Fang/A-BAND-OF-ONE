#include "abo_host/host_link.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "cJSON.h"
#include "abo_host/wire_messages.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"

namespace abo_host {
namespace {

constexpr const char* kTag = "abo_host";

const char* mode_name(abo_p2::PerformanceMode mode) {
  return mode == abo_p2::PerformanceMode::Score ? "score" : "free";
}

const char* phase_name(abo_p2::PerformancePhase phase) {
  switch (phase) {
    case abo_p2::PerformancePhase::Standby: return "standby";
    case abo_p2::PerformancePhase::Playing: return "playing";
    case abo_p2::PerformancePhase::Finished: return "finished";
  }
  return "standby";
}

const char* subphase_name(abo_p2::PerformanceSubphase subphase) {
  switch (subphase) {
    case abo_p2::PerformanceSubphase::None: return "none";
    case abo_p2::PerformanceSubphase::Waiting: return "waiting";
    case abo_p2::PerformanceSubphase::Holding: return "holding";
  }
  return "none";
}

const cJSON* field(const cJSON* object, const char* name) {
  return cJSON_GetObjectItemCaseSensitive(object, name);
}

bool number_field(const cJSON* object, const char* name, double* value) {
  const auto* item = field(object, name);
  if (item == nullptr || !cJSON_IsNumber(item) || value == nullptr ||
      !std::isfinite(item->valuedouble)) {
    return false;
  }
  *value = item->valuedouble;
  return true;
}

bool integer_field(const cJSON* object, const char* name, std::uint16_t* value) {
  double parsed = 0;
  if (!number_field(object, name, &parsed) || parsed < 0 ||
      parsed > 65535 || std::floor(parsed) != parsed || value == nullptr) {
    return false;
  }
  *value = static_cast<std::uint16_t>(parsed);
  return true;
}

bool string_field(const cJSON* object, const char* name, const char** value) {
  const auto* item = field(object, name);
  if (item == nullptr || !cJSON_IsString(item) || item->valuestring == nullptr ||
      value == nullptr) {
    return false;
  }
  *value = item->valuestring;
  return true;
}

bool crc_field(const cJSON* object, const char* name, std::uint32_t* value) {
  const char* text = nullptr;
  if (!string_field(object, name, &text) || std::strlen(text) != 8 ||
      value == nullptr) {
    return false;
  }
  std::uint32_t parsed = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    const char byte = text[i];
    std::uint8_t nibble = 0;
    if (byte >= '0' && byte <= '9') {
      nibble = static_cast<std::uint8_t>(byte - '0');
    } else if (byte >= 'a' && byte <= 'f') {
      nibble = static_cast<std::uint8_t>(byte - 'a' + 10);
    } else {
      return false;
    }
    parsed = (parsed << 4) | nibble;
  }
  *value = parsed;
  return true;
}

bool valid_transaction_text(const char* text) {
  if (text == nullptr || std::strlen(text) != kMaxTransactionLength) {
    return false;
  }
  for (std::size_t index = 0; index < kMaxTransactionLength; ++index) {
    const char byte = text[index];
    const bool digit = byte >= '0' && byte <= '9';
    const bool lower = byte >= 'a' && byte <= 'f';
    if (!digit && !lower) return false;
  }
  return true;
}

bool same_transaction_text(
    const char* text,
    const std::array<char, kMaxTransactionLength>& stored) {
  if (text == nullptr) return false;
  for (std::size_t index = 0; index < stored.size(); ++index) {
    if (text[index] != stored[index]) return false;
  }
  return text[kMaxTransactionLength] == '\0';
}

const char* protocol_error_name(ProtocolError error) {
  switch (error) {
    case ProtocolError::None: return "ok";
    case ProtocolError::InvalidVersion: return "invalid_version";
    case ProtocolError::InvalidSchema: return "invalid_schema";
    case ProtocolError::InvalidTransaction: return "invalid_tx";
    case ProtocolError::InvalidId: return "invalid_id";
    case ProtocolError::InvalidTitle: return "invalid_title";
    case ProtocolError::InvalidBpm: return "invalid_bpm";
    case ProtocolError::InvalidCount: return "invalid_count";
    case ProtocolError::InvalidNote: return "invalid_note";
    case ProtocolError::NoActiveUpload: return "no_active_upload";
    case ProtocolError::TransactionMismatch: return "tx_mismatch";
    case ProtocolError::SequenceMismatch: return "seq_mismatch";
    case ProtocolError::ChunkTooLarge: return "chunk_too_large";
    case ProtocolError::NoteCountExceeded: return "note_count_exceeded";
    case ProtocolError::ChunkCountMismatch: return "chunk_count_mismatch";
    case ProtocolError::NoteCountMismatch: return "note_count_mismatch";
    case ProtocolError::CrcMismatch: return "crc_mismatch";
    case ProtocolError::InvalidMode: return "invalid_mode";
    case ProtocolError::ModeNotStandby: return "mode_not_standby";
    case ProtocolError::InvalidVolume: return "invalid_volume";
    case ProtocolError::VolumeUnavailable: return "volume_unavailable";
    case ProtocolError::InvalidTransportAction:
      return "invalid_transport_action";
    case ProtocolError::TransportNotFinished:
      return "transport_not_finished";
  }
  return "protocol_error";
}

}  // namespace

esp_err_t HostLink::begin() {
  if (ready_) return ESP_OK;
  auto config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  config.tx_buffer_size = 1024;
  config.rx_buffer_size = 1024;
  const esp_err_t error = usb_serial_jtag_driver_install(&config);
  if (error != ESP_OK) {
    ESP_LOGE(kTag, "USB-Serial/JTAG install failed: %s", esp_err_to_name(error));
    return error;
  }
  rx_length_ = 0;
  rx_overflow_ = false;
  clear_tx();
  next_input_sequence_ = 1U;
  has_last_transport_tx_ = false;
  ready_ = true;
  ESP_LOGI(kTag, "A Band of One USB-Serial/JTAG host link ready protocol=1");
  return ESP_OK;
}

void HostLink::set_mode_request_handler(ModeRequestHandler handler,
                                         void* context) {
  mode_request_handler_ = handler;
  mode_request_context_ = context;
}

void HostLink::set_volume_request_handler(VolumeRequestHandler handler,
                                           void* context) {
  volume_request_handler_ = handler;
  volume_request_context_ = context;
}

void HostLink::set_transport_reset_handler(TransportResetHandler handler,
                                            void* context) {
  transport_reset_handler_ = handler;
  transport_reset_context_ = context;
}

bool HostLink::enqueue_line(const char* line, TxPriority priority) {
  if (line == nullptr) return false;
  const auto length = std::strlen(line);
  if (length == 0 || length > kMaxLineBytes + 1) return false;

  if (priority == TxPriority::State) {
    for (std::size_t index = 0; index < length; ++index) {
      state_tx_.bytes[index] = line[index];
    }
    state_tx_.length = length;
    state_tx_.offset = 0U;
    state_tx_pending_ = true;
    return true;
  }

  if (critical_tx_size_ >= kCriticalTxCapacity) {
    ++tx_drop_count_;
    ESP_LOGW(kTag, "critical TX queue full drops=%lu",
             static_cast<unsigned long>(tx_drop_count_));
    return false;
  }
  auto& message = critical_tx_[critical_tx_tail_];
  for (std::size_t index = 0; index < length; ++index) {
    message.bytes[index] = line[index];
  }
  message.length = length;
  message.offset = 0U;
  critical_tx_tail_ = (critical_tx_tail_ + 1U) % kCriticalTxCapacity;
  ++critical_tx_size_;
  return true;
}

void HostLink::send_line(const char* line, TxPriority priority) {
  if (!ready_ || line == nullptr) return;
  enqueue_line(line, priority);
}

void HostLink::flush_tx() {
  if (!ready_) return;

  while (critical_tx_size_ > 0U) {
    auto& message = critical_tx_[critical_tx_head_];
    const auto remaining = message.length - message.offset;
    const int written = usb_serial_jtag_write_bytes(
        message.bytes.data() + message.offset, remaining, 0);
    if (written <= 0) return;
    if (static_cast<std::size_t>(written) > remaining) {
      ++tx_short_write_count_;
      return;
    }
    message.offset += static_cast<std::size_t>(written);
    if (message.offset < message.length) {
      ++tx_short_write_count_;
      return;
    }
    message = TxMessage{};
    critical_tx_head_ = (critical_tx_head_ + 1U) % kCriticalTxCapacity;
    --critical_tx_size_;
  }

  if (!state_tx_pending_) return;
  const auto remaining = state_tx_.length - state_tx_.offset;
  const int written = usb_serial_jtag_write_bytes(
      state_tx_.bytes.data() + state_tx_.offset, remaining, 0);
  if (written <= 0) return;
  if (static_cast<std::size_t>(written) > remaining) {
    ++tx_short_write_count_;
    return;
  }
  state_tx_.offset += static_cast<std::size_t>(written);
  if (state_tx_.offset == state_tx_.length) {
    state_tx_ = TxMessage{};
    state_tx_pending_ = false;
  } else {
    ++tx_short_write_count_;
  }
}

void HostLink::clear_tx() {
  critical_tx_head_ = 0U;
  critical_tx_tail_ = 0U;
  critical_tx_size_ = 0U;
  state_tx_ = TxMessage{};
  state_tx_pending_ = false;
}

void HostLink::send_hello() {
  send_line("{\"t\":\"hello\",\"v\":1,\"product\":\"abo\",\"protocol\":1,\"firmware\":\"0.1.0-abo\"}\n");
}

void HostLink::send_input(std::string_view control, std::string_view phase) {
  if (!ready_) return;
  char line[128]{};
  const auto sequence = next_input_sequence_++;
  if (format_input(line, sizeof(line), sequence, control, phase) > 0) {
    send_line(line, TxPriority::Critical);
  }
}

void HostLink::send_state() {
  const Score* score = score_protocol_.committed_score();
  const std::string_view score_id =
      p2_state_active_
          ? std::string_view(p2_score_id_.data(), p2_score_id_length_)
          : (score == nullptr
                 ? std::string_view{}
                 : std::string_view(score->id.data(), score->id_length));
  const WireStateView state = p2_state_active_
                                  ? WireStateView{
                                        .mode = mode_name(p2_state_.mode),
                                        .phase = phase_name(p2_state_.phase),
                                        .subphase = subphase_name(p2_state_.subphase),
                                        .instrument = p2_state_.instrument,
                                        .instrument_loading = instrument_loading_,
                                        .octave = p2_state_.octave,
                                        .bpm = p2_state_.bpm,
                                        .errors = p2_state_.errors,
                                        .cursor = p2_state_.cursor,
                                        .elapsed = p2_elapsed_ms_,
                                        .elapsed_ms = p2_elapsed_ms_,
                                        .score_crc32 = p2_score_crc32_,
                                        .note_ticks = p2_note_ticks_,
                                        .note_total_ticks = p2_note_total_ticks_,
                                        .knob = p2_knob_,
                                        .volume = p2_state_.volume,
                                        .leds = p2_leds_,
                                        .keys = key_states_.data(),
                                        .key_count = key_states_.size(),
                                        .score_loaded = p2_score_loaded_,
                                        .score_id = score_id,
                                        .score_version = 1U,
                                    }
                                  : WireStateView{
                                        .mode = "free",
                                        .phase = "standby",
                                        .subphase = "none",
                                        .instrument = instrument_,
                                        .instrument_loading = instrument_loading_,
                                        .octave = octave_,
                                        .bpm = 90,
                                        .errors = 0,
                                        .elapsed = 0,
                                        .elapsed_ms = 0,
                                        .score_crc32 = score == nullptr
                                                           ? 0U
                                                           : ScoreProtocol::canonical_crc32(*score),
                                        .note_ticks = 0U,
                                        .note_total_ticks = 0U,
                                        .knob = knob_,
                                        .volume = volume_,
                                        .leds = {false, false, false, false, false},
                                        .keys = key_states_.data(),
                                        .key_count = key_states_.size(),
                                        .score_loaded = score != nullptr,
                                        .score_id = score_id,
                                        .score_version = static_cast<std::uint8_t>(
                                            score == nullptr ? 0U
                                                             : static_cast<unsigned>(score->score_version)),
                                    };
  char line[kMaxLineBytes + 1]{};
  if (format_state(line, sizeof(line), state) > 0) {
    send_line(line, TxPriority::State);
  }
}

void HostLink::set_performance_state(std::uint8_t instrument,
                                     std::uint8_t octave,
                                     std::int16_t knob,
                                     bool instrument_loading,
                                     std::uint8_t volume) {
  instrument_ = instrument;
  octave_ = octave;
  knob_ = knob;
  instrument_loading_ = instrument_loading;
  volume_ = volume > 100U ? 100U : volume;
}

void HostLink::set_p2_state(const abo_p2::PerformanceState& state,
                            std::string_view score_id,
                            bool score_loaded,
                            const std::array<bool, 5>& leds,
                            bool instrument_loading,
                            std::int16_t knob,
                            std::uint32_t elapsed_ms,
                            std::uint32_t score_crc32,
                            std::uint32_t note_ticks,
                            std::uint32_t note_total_ticks) {
  p2_state_ = state;
  p2_state_active_ = true;
  p2_leds_ = leds;
  p2_score_loaded_ = score_loaded;
  p2_score_id_length_ = static_cast<std::uint8_t>(
      score_id.size() > p2_score_id_.size() - 1U ? p2_score_id_.size() - 1U
                                                  : score_id.size());
  p2_score_id_.fill('\0');
  for (std::size_t i = 0U; i < p2_score_id_length_; ++i) {
    p2_score_id_[i] = score_id[i];
  }
  instrument_loading_ = instrument_loading;
  p2_knob_ = knob;
  p2_elapsed_ms_ = elapsed_ms;
  p2_score_crc32_ = score_crc32;
  p2_note_ticks_ = note_ticks;
  p2_note_total_ticks_ = note_total_ticks;
}

void HostLink::set_key_states(const std::array<const char*, 8>& states) {
  key_states_ = states;
}

void HostLink::send_judge(bool ok,
                          std::uint8_t target,
                          std::uint8_t expected,
                          std::uint16_t errors) {
  char line[192]{};
  if (format_judge(line, sizeof(line), ok, target, expected, errors) > 0) {
    send_line(line);
  }
}

void HostLink::send_result(std::uint16_t errors,
                           std::uint32_t elapsed_ms) {
  char line[160]{};
  if (format_result(line, sizeof(line), errors, elapsed_ms) > 0) {
    send_line(line);
  }
}

void HostLink::send_runtime_error(const char* reason) {
  send_error("runtime", "", reason == nullptr ? "runtime_error" : reason);
}

void HostLink::send_key(std::uint8_t index, bool down) {
  if (index >= key_states_.size()) return;
  key_states_[index] = down ? "holding" : "idle";
  send_state();
}

void HostLink::send_ack(const char* command, const char* tx) {
  char line[256]{};
  const std::string_view command_view = command == nullptr ? "" : command;
  const std::string_view tx_view = tx == nullptr ? "" : tx;
  if (format_ack(line, sizeof(line), command_view, tx_view) > 0) {
    send_line(line);
  }
}

void HostLink::send_score_commit_ack(const char* tx) {
  const Score* score = score_protocol_.committed_score();
  if (score == nullptr) {
    send_ack("score_commit", tx);
    return;
  }
  char line[256]{};
  const std::string_view tx_view = tx == nullptr ? "" : tx;
  const std::string_view score_id(score->id.data(), score->id_length);
  if (format_score_commit_ack(line, sizeof(line), tx_view, score_id,
                              score->score_version) > 0) {
    send_line(line);
  }
}

void HostLink::send_error(const char* command,
                          const char* tx,
                          const char* reason) {
  char line[176];
  std::snprintf(line, sizeof(line),
                "{\"t\":\"error\",\"v\":1,\"cmd\":\"%s\",\"tx\":\"%s\",\"reason\":\"%s\"}\n",
                command == nullptr ? "" : command, tx == nullptr ? "" : tx,
                reason == nullptr ? "invalid_command" : reason);
  send_line(line);
}

void HostLink::process_line(const char* line, std::size_t length) {
  if (line == nullptr || length == 0) return;
  cJSON* root = cJSON_ParseWithLength(line, length);
  if (root == nullptr || !cJSON_IsObject(root)) {
    cJSON_Delete(root);
    send_error("line", "", "invalid_json");
    return;
  }
  const char* command = nullptr;
  const char* tx = "";
  string_field(root, "t", &command);
  string_field(root, "tx", &tx);
  double version = 0;
  if (command == nullptr || !number_field(root, "v", &version) || version != 1) {
    cJSON_Delete(root);
    send_error(command == nullptr ? "line" : command, tx, "invalid_version");
    return;
  }

  if (std::strcmp(command, "ping") == 0) {
    send_hello();
    send_state();
    cJSON_Delete(root);
    return;
  }

  ProtocolError error = ProtocolError::None;
  if (std::strcmp(command, "set_mode") == 0) {
    const char* requested_mode = nullptr;
    if (!valid_transaction_text(tx) ||
        !string_field(root, "mode", &requested_mode)) {
      error = ProtocolError::InvalidMode;
    } else {
      abo_p2::PerformanceMode mode{};
      if (std::strcmp(requested_mode, "score") == 0) {
        mode = abo_p2::PerformanceMode::Score;
      } else if (std::strcmp(requested_mode, "free") == 0) {
        mode = abo_p2::PerformanceMode::Free;
      } else {
        error = ProtocolError::InvalidMode;
      }
      if (error == ProtocolError::None &&
          (mode_request_handler_ == nullptr ||
           !mode_request_handler_(mode, mode_request_context_))) {
        error = ProtocolError::ModeNotStandby;
      }
    }
  } else if (std::strcmp(command, "set_volume") == 0) {
    std::uint16_t requested_volume = 0;
    if (!valid_transaction_text(tx) ||
        !integer_field(root, "value", &requested_volume) ||
        requested_volume > 100U) {
      error = ProtocolError::InvalidVolume;
    } else if (volume_request_handler_ == nullptr ||
               !volume_request_handler_(
                   static_cast<std::uint8_t>(requested_volume),
                   volume_request_context_)) {
      error = ProtocolError::VolumeUnavailable;
    }
  } else if (std::strcmp(command, "transport") == 0) {
    const char* action = nullptr;
    if (!valid_transaction_text(tx)) {
      error = ProtocolError::InvalidTransaction;
    } else if (!string_field(root, "action", &action) ||
               std::strcmp(action, "reset") != 0) {
      error = ProtocolError::InvalidTransportAction;
    } else if (has_last_transport_tx_ &&
               same_transaction_text(tx, last_transport_tx_)) {
      // Replaying the same transaction is idempotent. The first request has
      // already performed the reset; acknowledge the retry without invoking
      // the state transition a second time.
      error = ProtocolError::None;
    } else if (transport_reset_handler_ == nullptr ||
               !transport_reset_handler_(transport_reset_context_)) {
      error = ProtocolError::TransportNotFinished;
    } else {
      for (std::size_t index = 0; index < last_transport_tx_.size(); ++index) {
        last_transport_tx_[index] = tx[index];
      }
      has_last_transport_tx_ = true;
    }
  } else if (std::strcmp(command, "score_begin") == 0) {
    const char *schema = nullptr, *id = nullptr, *title = nullptr;
    std::uint16_t score_version = 0, bpm = 0, count = 0;
    if (!string_field(root, "schema", &schema) ||
        !integer_field(root, "score_version", &score_version) ||
        !string_field(root, "id", &id) || !string_field(root, "title", &title) ||
        !integer_field(root, "bpm", &bpm) || !integer_field(root, "count", &count)) {
      error = ProtocolError::InvalidSchema;
    } else {
      error = score_protocol_.begin(BeginRequest{
          1, tx, schema, static_cast<std::uint8_t>(score_version), id, title,
          bpm, count});
    }
  } else if (std::strcmp(command, "score_chunk") == 0) {
    std::uint16_t sequence = 0;
    const auto* notes = field(root, "notes");
    std::array<NoteInput, kMaxChunkNotes> parsed{};
    if (!integer_field(root, "seq", &sequence) || notes == nullptr ||
        !cJSON_IsArray(notes) || cJSON_GetArraySize(notes) == 0) {
      error = ProtocolError::InvalidNote;
    } else if (cJSON_GetArraySize(notes) > static_cast<int>(kMaxChunkNotes)) {
      error = ProtocolError::ChunkTooLarge;
    } else {
      const auto item_count = static_cast<std::size_t>(cJSON_GetArraySize(notes));
      for (std::size_t i = 0; i < item_count; ++i) {
        const auto* item = cJSON_GetArrayItem(notes, static_cast<int>(i));
        std::uint16_t note = 0;
        double beats = 0;
        if (item == nullptr || !integer_field(item, "n", &note) || note > 6 ||
            !number_field(item, "b", &beats) || beats < 0.25 || beats > 8.0 ||
            std::floor(beats * 4.0) != beats * 4.0) {
          error = ProtocolError::InvalidNote;
          break;
        }
        parsed[i] = {static_cast<std::uint8_t>(note),
                     static_cast<std::uint16_t>(std::lround(beats * 96.0))};
      }
      if (error == ProtocolError::None) {
        error = score_protocol_.append_chunk(
            ChunkRequest{1, tx, sequence, parsed.data(), item_count});
      }
    }
  } else if (std::strcmp(command, "score_commit") == 0) {
    std::uint16_t chunks = 0;
    std::uint32_t crc32 = 0;
    if (!integer_field(root, "chunks", &chunks) ||
        !crc_field(root, "crc32", &crc32)) {
      error = ProtocolError::CrcMismatch;
    } else {
      error = score_protocol_.commit(CommitRequest{1, tx, chunks, crc32});
    }
  } else {
    error = ProtocolError::InvalidSchema;
  }

  if (error == ProtocolError::None) {
    if (std::strcmp(command, "score_commit") == 0) {
      send_score_commit_ack(tx);
      send_state();
    } else if (std::strcmp(command, "transport") == 0) {
      send_ack(command, tx);
      send_state();
    } else {
      send_ack(command, tx);
    }
  } else {
    send_error(command, tx, protocol_error_name(error));
  }
  cJSON_Delete(root);
}

void HostLink::poll() {
  if (!ready_) return;
  flush_tx();
  std::array<char, kReadChunkBytes> chunk{};
  const int count = usb_serial_jtag_read_bytes(chunk.data(), chunk.size(), 0);
  if (count <= 0) return;
  for (int i = 0; i < count; ++i) {
    const char byte = chunk[static_cast<std::size_t>(i)];
    if (byte == '\r') continue;
    if (byte == '\n') {
      if (rx_overflow_) {
        send_error("line", "", "line_too_long");
      } else if (rx_length_ > 0) {
        rx_buffer_[rx_length_] = '\0';
        process_line(rx_buffer_.data(), rx_length_);
      }
      rx_length_ = 0;
      rx_overflow_ = false;
      continue;
    }
    if (rx_overflow_) continue;
    if (rx_length_ >= kMaxLineBytes) {
      rx_overflow_ = true;
      continue;
    }
    rx_buffer_[rx_length_++] = byte;
  }
  flush_tx();
}

}  // namespace abo_host
