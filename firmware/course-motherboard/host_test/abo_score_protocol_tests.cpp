#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <string_view>

#include "abo_host/score_protocol.h"

namespace {

using abo_host::BeginRequest;
using abo_host::ChunkRequest;
using abo_host::CommitRequest;
using abo_host::NoteInput;
using abo_host::ProtocolError;
using abo_host::Score;
using abo_host::ScoreProtocol;

BeginRequest begin_request(std::string_view tx,
                           std::string_view id = "demo",
                           std::string_view title = "Demo",
                           uint16_t bpm = 90,
                           uint16_t count = 1) {
  return BeginRequest{
      .v = 1,
      .tx = tx,
      .schema = "abo.score",
      .score_version = 1,
      .id = id,
      .title_utf8 = title,
      .bpm = bpm,
      .count = count,
  };
}

ProtocolError append_one(ScoreProtocol& protocol,
                         std::string_view tx,
                         uint16_t seq,
                         uint8_t note = 0,
                         uint16_t ticks = 96) {
  const NoteInput input{.n = note, .ticks = ticks};
  return protocol.append_chunk(ChunkRequest{
      .v = 1,
      .tx = tx,
      .seq = seq,
      .notes = &input,
      .note_count = 1,
  });
}

Score expected_score(std::string_view id,
                     std::string_view title,
                     uint16_t bpm,
                     std::initializer_list<NoteInput> notes) {
  Score score{};
  score.score_version = 1;
  score.id_length = static_cast<uint8_t>(id.size());
  for (size_t i = 0; i < id.size(); ++i) score.id[i] = id[i];
  score.title_length = static_cast<uint8_t>(title.size());
  for (size_t i = 0; i < title.size(); ++i) score.title_utf8[i] = title[i];
  score.bpm = bpm;
  score.note_count = static_cast<uint16_t>(notes.size());
  size_t index = 0;
  for (const NoteInput note : notes) {
    score.notes[index++] = abo_host::Note{.n = note.n, .ticks = note.ticks};
  }
  return score;
}

void assert_current_note(const ScoreProtocol& protocol, uint8_t note) {
  assert(protocol.has_committed_score());
  assert(protocol.committed_score()->note_count == 1);
  assert(protocol.committed_score()->notes[0].n == note);
}

void establish_base_score(ScoreProtocol& protocol) {
  assert(protocol.begin(begin_request("11111111")) == ProtocolError::None);
  assert(append_one(protocol, "11111111", 0, 0) == ProtocolError::None);
  const Score base = expected_score("demo", "Demo", 90, {{0, 96}});
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "11111111",
             .chunks = 1,
             .crc32 = ScoreProtocol::canonical_crc32(base),
         }) == ProtocolError::None);
  assert_current_note(protocol, 0);
}

void test_fixed_crc_vector() {
  ScoreProtocol protocol;
  assert(protocol.begin(BeginRequest{
             .v = 1,
             .tx = "7fa31c09",
             .schema = "abo.score",
             .score_version = 1,
             .id = "crc",
             .title_utf8 = "A",
             .bpm = 60,
             .count = 1,
         }) == ProtocolError::None);
  assert(append_one(protocol, "7fa31c09", 0, 0, 96) == ProtocolError::None);
  const Score score = expected_score("crc", "A", 60, {{0, 96}});
  assert(ScoreProtocol::canonical_crc32(score) == 0xa60bb9dfU);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "7fa31c09",
             .chunks = 1,
             .crc32 = 0xa60bb9dfU,
         }) == ProtocolError::None);
}

void test_begin_validator() {
  ScoreProtocol protocol;
  auto request = begin_request("12345678");
  request.schema = "wrong";
  assert(protocol.begin(request) == ProtocolError::InvalidSchema);
  request = begin_request("12345678");
  request.v = 2;
  assert(protocol.begin(request) == ProtocolError::InvalidVersion);
  request = begin_request("1234567G");
  assert(protocol.begin(request) == ProtocolError::InvalidTransaction);
  request = begin_request("12345678", "含中文");
  assert(protocol.begin(request) == ProtocolError::InvalidId);
  request = begin_request("12345678", "demo", "");
  assert(protocol.begin(request) == ProtocolError::InvalidTitle);
  request = begin_request("12345678", "demo", "Demo", 59);
  assert(protocol.begin(request) == ProtocolError::InvalidBpm);
  request = begin_request("12345678", "demo", "Demo", 90, 0);
  assert(protocol.begin(request) == ProtocolError::InvalidCount);
}

void test_transaction_order_and_bounds() {
  ScoreProtocol protocol;
  establish_base_score(protocol);

  assert(protocol.begin(begin_request("22222222", "next", "Next", 100, 2)) ==
         ProtocolError::None);
  assert(append_one(protocol, "wrongtx", 0) == ProtocolError::TransactionMismatch);
  assert(append_one(protocol, "22222222", 1) == ProtocolError::SequenceMismatch);
  assert(append_one(protocol, "22222222", 0, 1) == ProtocolError::None);
  assert(append_one(protocol, "22222222", 0, 2) == ProtocolError::SequenceMismatch);
  assert_current_note(protocol, 0);

  std::array<NoteInput, 17> too_many{};
  assert(protocol.append_chunk(ChunkRequest{
             .v = 1,
             .tx = "22222222",
             .seq = 1,
             .notes = too_many.data(),
             .note_count = too_many.size(),
         }) == ProtocolError::ChunkTooLarge);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "22222222",
             .chunks = 2,
             .crc32 = 0,
         }) == ProtocolError::ChunkCountMismatch);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "22222222",
             .chunks = 1,
             .crc32 = 0,
         }) == ProtocolError::NoteCountMismatch);
  assert_current_note(protocol, 0);
}

void test_commit_failures_preserve_old_score() {
  ScoreProtocol protocol;
  establish_base_score(protocol);

  assert(protocol.begin(begin_request("33333333", "bad", "Bad", 110)) ==
         ProtocolError::None);
  assert(append_one(protocol, "33333333", 0, 3) == ProtocolError::None);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "33333333",
             .chunks = 2,
             .crc32 = 0,
         }) == ProtocolError::ChunkCountMismatch);
  assert_current_note(protocol, 0);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "33333333",
             .chunks = 1,
             .crc32 = 0,
         }) == ProtocolError::CrcMismatch);
  assert_current_note(protocol, 0);
}

void test_new_begin_replaces_only_staging() {
  ScoreProtocol protocol;
  establish_base_score(protocol);

  assert(protocol.begin(begin_request("44444444", "old", "Old")) ==
         ProtocolError::None);
  assert(append_one(protocol, "44444444", 0, 4) == ProtocolError::None);
  assert_current_note(protocol, 0);

  assert(protocol.begin(begin_request("55555555", "new", "New", 120)) ==
         ProtocolError::None);
  assert(append_one(protocol, "55555555", 0, 5) == ProtocolError::None);
  const Score replacement = expected_score("new", "New", 120, {{5, 96}});
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "55555555",
             .chunks = 1,
             .crc32 = ScoreProtocol::canonical_crc32(replacement),
         }) == ProtocolError::None);
  assert_current_note(protocol, 5);
}

void test_note_validator_and_no_session() {
  ScoreProtocol protocol;
  assert(append_one(protocol, "12345678", 0) == ProtocolError::NoActiveUpload);
  assert(protocol.begin(begin_request("66666666")) == ProtocolError::None);
  assert(append_one(protocol, "66666666", 0, 7) == ProtocolError::InvalidNote);
  assert(append_one(protocol, "66666666", 0, 0, 23) == ProtocolError::InvalidNote);
  assert(append_one(protocol, "66666666", 0, 0, 97) == ProtocolError::InvalidNote);
}

}  // namespace

int main() {
  test_fixed_crc_vector();
  test_begin_validator();
  test_transaction_order_and_bounds();
  test_commit_failures_preserve_old_score();
  test_new_begin_replaces_only_staging();
  test_note_validator_and_no_session();
}
