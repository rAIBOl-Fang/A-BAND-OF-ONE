#include "abo_host/score_protocol.h"

namespace abo_host {
namespace {

constexpr uint16_t kMinBpm = 60;
constexpr uint16_t kMaxBpm = 180;
constexpr uint16_t kMinTicks = 24;
constexpr uint16_t kMaxTicks = 768;
constexpr uint16_t kTicksStep = 24;

uint32_t crc32_update(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (int bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc;
}

uint32_t crc32_bytes(uint32_t crc, const uint8_t* bytes, size_t count) {
  for (size_t i = 0; i < count; ++i) crc = crc32_update(crc, bytes[i]);
  return crc;
}

uint32_t crc32_u16(uint32_t crc, uint16_t value) {
  const uint8_t bytes[2] = {
      static_cast<uint8_t>(value & 0xffU),
      static_cast<uint8_t>((value >> 8) & 0xffU),
  };
  return crc32_bytes(crc, bytes, 2);
}

}  // namespace

bool ScoreProtocol::valid_transaction(std::string_view tx) {
  if (tx.size() != kMaxTransactionLength) return false;
  for (const char byte : tx) {
    const bool digit = byte >= '0' && byte <= '9';
    const bool lower = byte >= 'a' && byte <= 'f';
    if (!digit && !lower) return false;
  }
  return true;
}

bool ScoreProtocol::same_transaction(
    std::string_view tx,
    const std::array<char, kMaxTransactionLength>& stored) {
  if (tx.size() != stored.size()) return false;
  for (size_t i = 0; i < stored.size(); ++i) {
    if (tx[i] != stored[i]) return false;
  }
  return true;
}

bool ScoreProtocol::valid_id(std::string_view id) {
  if (id.empty() || id.size() > kMaxIdBytes) return false;
  for (const unsigned char byte : id) {
    if (byte < 0x21U || byte > 0x7eU) return false;
  }
  return true;
}

bool ScoreProtocol::valid_title(std::string_view title) {
  if (title.empty() || title.size() > kMaxTitleBytes) return false;
  for (const unsigned char byte : title) {
    if (byte == 0U) return false;
  }
  return true;
}

bool ScoreProtocol::valid_note(const NoteInput& note) {
  return note.n <= 6 && note.ticks >= kMinTicks &&
         note.ticks <= kMaxTicks && note.ticks % kTicksStep == 0;
}

ProtocolError ScoreProtocol::begin(const BeginRequest& request) {
  if (request.v != kProtocolVersion) return ProtocolError::InvalidVersion;
  if (request.schema != "abo.score") return ProtocolError::InvalidSchema;
  if (!valid_transaction(request.tx)) return ProtocolError::InvalidTransaction;
  if (request.score_version != 1) return ProtocolError::InvalidVersion;
  if (!valid_id(request.id)) return ProtocolError::InvalidId;
  if (!valid_title(request.title_utf8)) return ProtocolError::InvalidTitle;
  if (request.bpm < kMinBpm || request.bpm > kMaxBpm) {
    return ProtocolError::InvalidBpm;
  }
  if (request.count == 0 || request.count > kMaxNotes) {
    return ProtocolError::InvalidCount;
  }

  staging_ = Score{};
  staging_.score_version = request.score_version;
  staging_.id_length = static_cast<uint8_t>(request.id.size());
  for (size_t i = 0; i < request.id.size(); ++i) staging_.id[i] = request.id[i];
  staging_.title_length = static_cast<uint8_t>(request.title_utf8.size());
  for (size_t i = 0; i < request.title_utf8.size(); ++i) {
    staging_.title_utf8[i] = static_cast<uint8_t>(request.title_utf8[i]);
  }
  staging_.bpm = request.bpm;
  expected_count_ = request.count;
  received_count_ = 0;
  next_sequence_ = 0;
  for (size_t i = 0; i < transaction_.size(); ++i) {
    transaction_[i] = request.tx[i];
  }
  active_ = true;
  return ProtocolError::None;
}

ProtocolError ScoreProtocol::append_chunk(const ChunkRequest& request) {
  if (!active_) return ProtocolError::NoActiveUpload;
  if (request.v != kProtocolVersion) return ProtocolError::InvalidVersion;
  if (!same_transaction(request.tx, transaction_)) {
    return ProtocolError::TransactionMismatch;
  }
  if (request.seq != next_sequence_) return ProtocolError::SequenceMismatch;
  if (request.note_count == 0 || request.notes == nullptr) {
    return ProtocolError::InvalidNote;
  }
  if (request.note_count > kMaxChunkNotes) return ProtocolError::ChunkTooLarge;
  if (received_count_ + request.note_count > expected_count_) {
    return ProtocolError::NoteCountExceeded;
  }
  for (size_t i = 0; i < request.note_count; ++i) {
    if (!valid_note(request.notes[i])) return ProtocolError::InvalidNote;
  }
  for (size_t i = 0; i < request.note_count; ++i) {
    const size_t destination = received_count_ + i;
    staging_.notes[destination] = Note{
        .n = request.notes[i].n,
        .ticks = request.notes[i].ticks,
    };
  }
  received_count_ = static_cast<uint16_t>(received_count_ + request.note_count);
  staging_.note_count = received_count_;
  ++next_sequence_;
  return ProtocolError::None;
}

ProtocolError ScoreProtocol::commit(const CommitRequest& request) {
  if (!active_) return ProtocolError::NoActiveUpload;
  if (request.v != kProtocolVersion) return ProtocolError::InvalidVersion;
  if (!same_transaction(request.tx, transaction_)) {
    return ProtocolError::TransactionMismatch;
  }
  if (request.chunks != next_sequence_) return ProtocolError::ChunkCountMismatch;
  if (received_count_ != expected_count_) return ProtocolError::NoteCountMismatch;
  if (request.crc32 != canonical_crc32(staging_)) return ProtocolError::CrcMismatch;

  committed_ = staging_;
  has_committed_ = true;
  active_ = false;
  return ProtocolError::None;
}

uint32_t ScoreProtocol::canonical_crc32(const Score& score) {
  uint32_t crc = 0xffffffffU;
  constexpr char kSchema[] = "abo.score";
  crc = crc32_bytes(crc, reinterpret_cast<const uint8_t*>(kSchema), 9);
  crc = crc32_update(crc, 0U);
  crc = crc32_update(crc, score.score_version);
  crc = crc32_update(crc, score.id_length);
  crc = crc32_bytes(crc, reinterpret_cast<const uint8_t*>(score.id.data()),
                    score.id_length);
  crc = crc32_update(crc, score.title_length);
  crc = crc32_bytes(crc, score.title_utf8.data(), score.title_length);
  crc = crc32_u16(crc, score.bpm);
  crc = crc32_u16(crc, score.note_count);
  for (size_t i = 0; i < score.note_count; ++i) {
    crc = crc32_update(crc, score.notes[i].n);
    crc = crc32_u16(crc, score.notes[i].ticks);
  }
  return crc ^ 0xffffffffU;
}

}  // namespace abo_host
