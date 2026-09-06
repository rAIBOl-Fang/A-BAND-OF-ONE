#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "speaker_assets/abo_p1_sound_bank.h"

namespace {

using easy_input::speaker_assets::AboP1RootStorage;

struct ExpectedRoot {
  const char* id;
  easy_input::speaker_assets::AboP1RootStorage storage;
  std::uint8_t root_midi;
  std::size_t encoded_bytes;
  std::uint32_t decoded_samples;
  std::uint32_t loop_start_sample;
  std::uint32_t loop_end_sample;
};

constexpr std::array<ExpectedRoot, 7> kPianoRoots{{
    {"piano_b2", AboP1RootStorage::EiadV1, 47U, 49220U, 96000U, 36000U, 86400U},
    {"piano_fis3", AboP1RootStorage::EiadV1, 54U, 49220U, 96000U, 36000U, 86400U},
    {"piano_c4", AboP1RootStorage::EiadV1, 60U, 64214U, 125257U, 63360U, 125257U},
    {"piano_fis4", AboP1RootStorage::EiadV1, 66U, 49220U, 96000U, 36000U, 86400U},
    {"piano_c5", AboP1RootStorage::EiadV1, 72U, 49220U, 96000U, 36000U, 86400U},
    {"piano_fis5", AboP1RootStorage::EiadV1, 78U, 49220U, 96000U, 36000U, 86400U},
    {"piano_c6", AboP1RootStorage::EiadV1, 84U, 49220U, 96000U, 36000U, 86400U},
}};

constexpr std::array<ExpectedRoot, 7> kViolinRoots{{
    {"violin_g3", AboP1RootStorage::EiadV1, 55U, 36920U, 72000U, 33600U, 62400U},
    {"violin_c4", AboP1RootStorage::EiadV1, 60U, 36920U, 72000U, 33600U, 62400U},
    {"violin_e4", AboP1RootStorage::EiadV1, 64U, 36920U, 72000U, 33600U, 62400U},
    {"violin_g4", AboP1RootStorage::EiadV1, 67U, 36920U, 72000U, 33600U, 62400U},
    {"violin_c5", AboP1RootStorage::EiadV1, 72U, 36920U, 72000U, 33600U, 62400U},
    {"violin_e5", AboP1RootStorage::EiadV1, 76U, 36920U, 72000U, 33600U, 62400U},
    {"violin_a5", AboP1RootStorage::EiadV1, 81U, 36920U, 72000U, 33600U, 62400U},
}};

constexpr std::array<ExpectedRoot, 9> kClarinetRoots{{
    {"clarinet_d3", AboP1RootStorage::Pcm16Le, 50U, 76800U, 38400U, 20160U, 27984U},
    {"clarinet_f3", AboP1RootStorage::Pcm16Le, 53U, 76800U, 38400U, 20160U, 36912U},
    {"clarinet_as3", AboP1RootStorage::Pcm16Le, 58U, 76800U, 38400U, 20160U, 27984U},
    {"clarinet_d4", AboP1RootStorage::Pcm16Le, 62U, 76800U, 38400U, 20160U, 32400U},
    {"clarinet_f4", AboP1RootStorage::Pcm16Le, 65U, 76800U, 38400U, 20160U, 29472U},
    {"clarinet_as4", AboP1RootStorage::Pcm16Le, 70U, 76800U, 38400U, 20160U, 31056U},
    {"clarinet_d5", AboP1RootStorage::Pcm16Le, 74U, 76800U, 38400U, 20160U, 29376U},
    {"clarinet_f5", AboP1RootStorage::Pcm16Le, 77U, 76800U, 38400U, 20160U, 30432U},
    {"clarinet_as5", AboP1RootStorage::Pcm16Le, 82U, 76800U, 38400U, 20160U, 27552U},
}};

template <std::size_t N>
void assert_bank(std::uint8_t instrument_index,
                 const std::array<ExpectedRoot, N>& expected) {
  const auto* bank = easy_input::speaker_assets::abo_p1_sound_bank(instrument_index);
  assert(bank != nullptr);
  assert(bank->instrument_index == instrument_index);
  assert(bank->roots != nullptr);
  assert(bank->root_count == N);

  std::uint32_t decoded_samples_total = 0U;
  for (std::size_t i = 0; i < N; ++i) {
    const auto& actual = bank->roots[i];
    const auto& want = expected[i];
    assert(actual.id != nullptr);
    assert(std::strcmp(actual.id, want.id) == 0);
    assert(actual.storage == want.storage);
    assert(actual.root_midi == want.root_midi);
    assert(actual.payload != nullptr);
    assert(actual.payload_bytes == want.encoded_bytes);
    assert(actual.decoded_samples == want.decoded_samples);
    assert(actual.loop_start_sample == want.loop_start_sample);
    assert(actual.loop_end_sample == want.loop_end_sample);
    assert(actual.loop_start_sample < actual.loop_end_sample);
    assert(actual.loop_end_sample <= actual.decoded_samples);
    decoded_samples_total += actual.decoded_samples;
  }
  assert(bank->decoded_samples_total == decoded_samples_total);
}

}  // namespace

int main() {
  assert_bank(0U, kPianoRoots);
  assert_bank(1U, kViolinRoots);
  assert_bank(2U, kClarinetRoots);
  assert(easy_input::speaker_assets::abo_p1_sound_bank(3U) == nullptr);

  const auto piano_c4 = easy_input::speaker_assets::abo_p1_piano_c4_sound();
  assert(piano_c4.encoded != nullptr);
  assert(piano_c4.encoded_bytes == 64214U);
  assert(piano_c4.loop_start_frame == 132U);
  assert(piano_c4.loop_end_frame == 261U);
}
