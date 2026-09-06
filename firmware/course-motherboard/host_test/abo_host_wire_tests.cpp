#include <cassert>
#include <cstdint>
#include <string_view>

#include "abo_host/score_protocol.h"
#include "abo_host/wire_messages.h"

namespace {

using abo_host::BeginRequest;
using abo_host::ChunkRequest;
using abo_host::CommitRequest;
using abo_host::NoteInput;
using abo_host::ProtocolError;
using abo_host::Score;
using abo_host::ScoreProtocol;
using abo_host::WireStateView;

BeginRequest begin_request(std::string_view tx,
                           std::string_view id,
                           std::string_view title = "Demo",
                           uint16_t bpm = 90) {
  return BeginRequest{
      .v = 1,
      .tx = tx,
      .schema = "abo.score",
      .score_version = 1,
      .id = id,
      .title_utf8 = title,
      .bpm = bpm,
      .count = 1,
  };
}

void append_one(ScoreProtocol& protocol,
                std::string_view tx,
                uint8_t note) {
  const NoteInput input{.n = note, .ticks = 96};
  assert(protocol.append_chunk(ChunkRequest{
             .v = 1,
             .tx = tx,
             .seq = 0,
             .notes = &input,
             .note_count = 1,
         }) == ProtocolError::None);
}

Score score_for(std::string_view id, uint8_t note) {
  Score score{};
  score.score_version = 1;
  score.id_length = static_cast<uint8_t>(id.size());
  for (size_t i = 0; i < id.size(); ++i) score.id[i] = id[i];
  score.title_length = 4;
  score.title_utf8[0] = 'D';
  score.title_utf8[1] = 'e';
  score.title_utf8[2] = 'm';
  score.title_utf8[3] = 'o';
  score.bpm = 90;
  score.note_count = 1;
  score.notes[0] = abo_host::Note{.n = note, .ticks = 96};
  return score;
}

void test_commit_ack_exposes_score_identity() {
  char output[256]{};
  const size_t length = abo_host::format_score_commit_ack(
      output, sizeof(output), "a1b2c3d4", "hil-scale", 1);
  assert(length > 0);
  const std::string_view actual(output, length);
  assert(actual ==
         "{\"t\":\"ack\",\"v\":1,\"cmd\":\"score_commit\","
         "\"tx\":\"a1b2c3d4\",\"score_id\":\"hil-scale\","
         "\"score_version\":1}\n");
}

void test_state_exposes_committed_score_identity() {
  const char* keys[8] = {
      "idle", "idle", "idle", "idle", "idle", "idle", "idle", "idle"};
  WireStateView state{
      .mode = "free",
      .instrument = 0,
      .instrument_loading = true,
      .octave = 4,
      .bpm = 90,
      .errors = 0,
      .elapsed = 0,
      .knob = 0,
      .volume = 77,
      .leds = {false, false, false, false, false},
      .keys = keys,
      .key_count = 8,
      .score_loaded = true,
      .score_id = "hil-scale",
      .score_version = 1,
  };

  char output[512]{};
  const size_t length = abo_host::format_state(output, sizeof(output), state);
  assert(length > 0);
  const std::string_view actual(output, length);
  assert(actual.find("\"score_loaded\":true") != std::string_view::npos);
  assert(actual.find("\"score_id\":\"hil-scale\"") !=
         std::string_view::npos);
  assert(actual.find("\"score_version\":1") != std::string_view::npos);
  assert(actual.find("\"instrument_loading\":true") !=
         std::string_view::npos);
  assert(actual.find("\"volume\":77") != std::string_view::npos);
}

void test_p2_state_and_event_messages_match_console_contract() {
  const char* keys[8] = {
      "waiting", "idle", "idle", "idle", "idle", "idle", "idle", "idle"};
  WireStateView state{
      .mode = "score",
      .phase = "playing",
      .subphase = "holding",
      .instrument = 1,
      .instrument_loading = false,
      .octave = 5,
      .bpm = 120,
      .errors = 2,
      .cursor = 3,
      .elapsed = 1450,
      .elapsed_ms = 1450,
      .knob = 45,
      .volume = 70,
      .leds = {false, true, false, false, false},
      .keys = keys,
      .key_count = 8,
      .score_loaded = true,
      .score_id = "uploaded",
      .score_version = 1,
  };
  char output[768]{};
  const auto length = abo_host::format_state(output, sizeof(output), state);
  assert(length > 0);
  const std::string_view actual(output, length);
  assert(actual.find("\"phase\":\"playing\"") != std::string_view::npos);
  assert(actual.find("\"subphase\":\"holding\"") !=
         std::string_view::npos);
  assert(actual.find("\"cursor\":3") != std::string_view::npos);
  assert(actual.find("\"elapsed_ms\":1450") != std::string_view::npos);

  char judge[192]{};
  assert(abo_host::format_judge(judge, sizeof(judge), true, 2U, 2U, 2U) > 0);
  assert(std::string_view(judge).find("\"target\":2") !=
         std::string_view::npos);
  char result[192]{};
  assert(abo_host::format_result(result, sizeof(result), 2U, 1450U) > 0);
  assert(std::string_view(result).find("\"elapsed\":1450") !=
         std::string_view::npos);
}

void test_bad_crc_state_keeps_previous_score_identity() {
  ScoreProtocol protocol;
  assert(protocol.begin(begin_request("11111111", "hil-scale")) ==
         ProtocolError::None);
  append_one(protocol, "11111111", 0);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "11111111",
             .chunks = 1,
             .crc32 = ScoreProtocol::canonical_crc32(score_for("hil-scale", 0)),
         }) == ProtocolError::None);

  assert(protocol.begin(begin_request("22222222", "bad-score", "Bad")) ==
         ProtocolError::None);
  append_one(protocol, "22222222", 3);
  assert(protocol.commit(CommitRequest{
             .v = 1,
             .tx = "22222222",
             .chunks = 1,
             .crc32 = 0,
         }) == ProtocolError::CrcMismatch);

  const Score* committed = protocol.committed_score();
  assert(committed != nullptr);
  const std::string_view id(committed->id.data(), committed->id_length);
  assert(id == "hil-scale");

  const char* keys[8] = {
      "idle", "idle", "idle", "idle", "idle", "idle", "idle", "idle"};
  WireStateView state{
      .mode = "free",
      .instrument = 0,
      .instrument_loading = false,
      .octave = 4,
      .bpm = committed->bpm,
      .errors = 0,
      .elapsed = 0,
      .knob = 0,
      .volume = 70,
      .leds = {false, false, false, false, false},
      .keys = keys,
      .key_count = 8,
      .score_loaded = true,
      .score_id = id,
      .score_version = committed->score_version,
  };
  char output[512]{};
  const size_t length = abo_host::format_state(output, sizeof(output), state);
  assert(length > 0);
  const std::string_view actual(output, length);
  assert(actual.find("\"score_id\":\"hil-scale\"") !=
         std::string_view::npos);
  assert(actual.find("bad-score") == std::string_view::npos);
}

}  // namespace

int main() {
  test_commit_ack_exposes_score_identity();
  test_state_exposes_committed_score_identity();
  test_p2_state_and_event_messages_match_console_contract();
  test_bad_crc_state_keeps_previous_score_identity();
}
