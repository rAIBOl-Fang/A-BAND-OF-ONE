#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace abo_host {

constexpr uint8_t kProtocolVersion = 1;
constexpr size_t kMaxTransactionLength = 8;
constexpr size_t kMaxIdBytes = 32;
constexpr size_t kMaxTitleBytes = 48;
constexpr size_t kMaxNotes = 128;
constexpr size_t kMaxChunkNotes = 16;

struct BeginRequest {
  uint8_t v;
  std::string_view tx;
  std::string_view schema;
  uint8_t score_version;
  std::string_view id;
  std::string_view title_utf8;
  uint16_t bpm;
  uint16_t count;
};

struct NoteInput {
  uint8_t n;
  uint16_t ticks;
};

struct Note {
  uint8_t n;
  uint16_t ticks;
};

struct ChunkRequest {
  uint8_t v;
  std::string_view tx;
  uint16_t seq;
  const NoteInput* notes;
  size_t note_count;
};

struct CommitRequest {
  uint8_t v;
  std::string_view tx;
  uint16_t chunks;
  uint32_t crc32;
};

struct Score {
  uint8_t score_version = 0;
  std::array<char, kMaxIdBytes> id{};
  uint8_t id_length = 0;
  std::array<uint8_t, kMaxTitleBytes> title_utf8{};
  uint8_t title_length = 0;
  uint16_t bpm = 0;
  uint16_t note_count = 0;
  std::array<Note, kMaxNotes> notes{};
};

enum class ProtocolError {
  None,
  InvalidVersion,
  InvalidSchema,
  InvalidTransaction,
  InvalidId,
  InvalidTitle,
  InvalidBpm,
  InvalidCount,
  InvalidNote,
  NoActiveUpload,
  TransactionMismatch,
  SequenceMismatch,
  ChunkTooLarge,
  NoteCountExceeded,
  ChunkCountMismatch,
  NoteCountMismatch,
  CrcMismatch,
  InvalidMode,
  ModeNotStandby,
  InvalidVolume,
  VolumeUnavailable,
  InvalidTransportAction,
  TransportNotFinished,
};

class ScoreProtocol {
 public:
  ProtocolError begin(const BeginRequest& request);
  ProtocolError append_chunk(const ChunkRequest& request);
  ProtocolError commit(const CommitRequest& request);

  bool active() const { return active_; }
  bool has_committed_score() const { return has_committed_; }
  const Score* committed_score() const {
    return has_committed_ ? &committed_ : nullptr;
  }

  static uint32_t canonical_crc32(const Score& score);

 private:
  static bool valid_transaction(std::string_view tx);
  static bool same_transaction(std::string_view tx,
                               const std::array<char, kMaxTransactionLength>& stored);
  static bool valid_id(std::string_view id);
  static bool valid_title(std::string_view title);
  static bool valid_note(const NoteInput& note);

  std::array<char, kMaxTransactionLength> transaction_{};
  uint16_t next_sequence_ = 0;
  uint16_t expected_count_ = 0;
  uint16_t received_count_ = 0;
  bool active_ = false;
  bool has_committed_ = false;
  Score staging_{};
  Score committed_{};
};

}  // namespace abo_host
