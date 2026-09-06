#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "abo_p2/performance_types.h"
#include "abo_host/score_protocol.h"
#include "esp_err.h"

namespace abo_host {

class HostLink {
 public:
  using ModeRequestHandler = bool (*)(abo_p2::PerformanceMode mode,
                                      void* context);
  using VolumeRequestHandler = bool (*)(std::uint8_t volume, void* context);
  using TransportResetHandler = bool (*)(void* context);

  esp_err_t begin();
  void poll();
  bool ready() const { return ready_; }
  void set_mode_request_handler(ModeRequestHandler handler, void* context);
  void set_volume_request_handler(VolumeRequestHandler handler, void* context);
  void set_transport_reset_handler(TransportResetHandler handler,
                                   void* context);

  void send_hello();
  void send_state();
  void send_input(std::string_view control, std::string_view phase);
  void send_key(std::uint8_t index, bool down);
  void set_performance_state(std::uint8_t instrument,
                             std::uint8_t octave,
                             std::int16_t knob,
                             bool instrument_loading,
                             std::uint8_t volume = 70U);
  void set_p2_state(const abo_p2::PerformanceState& state,
                    std::string_view score_id,
                    bool score_loaded,
                    const std::array<bool, 5>& leds,
                    bool instrument_loading,
                    std::int16_t knob,
                    std::uint32_t elapsed_ms,
                    std::uint32_t score_crc32 = 0U,
                    std::uint32_t note_ticks = 0U,
                    std::uint32_t note_total_ticks = 0U);
  void set_key_states(const std::array<const char*, 8>& states);
  void send_judge(bool ok,
                  std::uint8_t target,
                  std::uint8_t expected,
                  std::uint16_t errors);
  void send_result(std::uint16_t errors, std::uint32_t elapsed_ms);
  void send_runtime_error(const char* reason);

  std::uint32_t tx_drop_count() const { return tx_drop_count_; }

  const ScoreProtocol& score_protocol() const { return score_protocol_; }
  ScoreProtocol& score_protocol() { return score_protocol_; }

 private:
  static constexpr std::size_t kMaxLineBytes = 767;
  static constexpr std::size_t kReadChunkBytes = 128;
  static constexpr std::size_t kCriticalTxCapacity = 16;

  enum class TxPriority {
    Critical,
    State,
  };

  struct TxMessage {
    std::array<char, kMaxLineBytes + 1> bytes{};
    std::size_t length = 0;
    std::size_t offset = 0;
  };

  void process_line(const char* line, std::size_t length);
  void send_ack(const char* command, const char* tx);
  void send_score_commit_ack(const char* tx);
  void send_error(const char* command, const char* tx, const char* reason);
  void send_line(const char* line, TxPriority priority = TxPriority::Critical);
  bool enqueue_line(const char* line, TxPriority priority);
  void flush_tx();
  void clear_tx();

  std::array<char, kMaxLineBytes + 1> rx_buffer_{};
  std::size_t rx_length_ = 0;
  bool rx_overflow_ = false;
  bool ready_ = false;
  std::uint8_t instrument_ = 0U;
  std::uint8_t octave_ = 4U;
  std::int16_t knob_ = 0;
  std::uint8_t volume_ = 70U;
  bool instrument_loading_ = false;
  bool p2_state_active_ = false;
  abo_p2::PerformanceState p2_state_{};
  std::array<bool, 5> p2_leds_{};
  std::array<char, 33> p2_score_id_{};
  std::uint8_t p2_score_id_length_ = 0U;
  std::uint32_t p2_elapsed_ms_ = 0U;
  std::int16_t p2_knob_ = 0;
  bool p2_score_loaded_ = false;
  std::uint32_t p2_score_crc32_ = 0U;
  std::uint32_t p2_note_ticks_ = 0U;
  std::uint32_t p2_note_total_ticks_ = 0U;
  ModeRequestHandler mode_request_handler_ = nullptr;
  void* mode_request_context_ = nullptr;
  VolumeRequestHandler volume_request_handler_ = nullptr;
  void* volume_request_context_ = nullptr;
  TransportResetHandler transport_reset_handler_ = nullptr;
  void* transport_reset_context_ = nullptr;
  std::array<const char*, 8> key_states_{{
      "idle", "idle", "idle", "idle", "idle", "idle", "idle", "idle"}};
  ScoreProtocol score_protocol_;
  std::array<TxMessage, kCriticalTxCapacity> critical_tx_{};
  std::size_t critical_tx_head_ = 0U;
  std::size_t critical_tx_tail_ = 0U;
  std::size_t critical_tx_size_ = 0U;
  TxMessage state_tx_{};
  bool state_tx_pending_ = false;
  std::uint32_t tx_drop_count_ = 0U;
  std::uint32_t tx_short_write_count_ = 0U;
  std::uint32_t next_input_sequence_ = 1U;
  std::array<char, kMaxTransactionLength> last_transport_tx_{};
  bool has_last_transport_tx_ = false;
};

}  // namespace abo_host
