#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "ima_adpcm_decoder.h"

namespace {

struct EiadAsset {
  const char* filename;
  std::uint32_t total_samples;
};

struct PcmAsset {
  const char* filename;
  std::uint32_t total_samples;
};

constexpr std::array<EiadAsset, 14> kEiadAssets{{
    {"piano_b2.eiad", 96000U},
    {"piano_fis3.eiad", 96000U},
    {"piano_c4.eiad", 125257U},
    {"piano_fis4.eiad", 96000U},
    {"piano_c5.eiad", 96000U},
    {"piano_fis5.eiad", 96000U},
    {"piano_c6.eiad", 96000U},
    {"violin_g3.eiad", 72000U},
    {"violin_c4.eiad", 72000U},
    {"violin_e4.eiad", 72000U},
    {"violin_g4.eiad", 72000U},
    {"violin_c5.eiad", 72000U},
    {"violin_e5.eiad", 72000U},
    {"violin_a5.eiad", 72000U},
}};

constexpr std::array<PcmAsset, 9> kPcmAssets{{
    {"clarinet_d3.pcm16le", 38400U},
    {"clarinet_f3.pcm16le", 38400U},
    {"clarinet_as3.pcm16le", 38400U},
    {"clarinet_d4.pcm16le", 38400U},
    {"clarinet_f4.pcm16le", 38400U},
    {"clarinet_as4.pcm16le", 38400U},
    {"clarinet_d5.pcm16le", 38400U},
    {"clarinet_f5.pcm16le", 38400U},
    {"clarinet_as5.pcm16le", 38400U},
}};

std::vector<std::uint8_t> read_asset(const char* filename) {
  const std::string path = std::string("abo_assets/delivery/") + filename;
  std::ifstream input(path, std::ios::binary);
  assert(input.good());
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void assert_eiad(const EiadAsset& asset) {
  const auto encoded = read_asset(asset.filename);
  assert(!encoded.empty());

  easy_input::ImaAdpcmDecoder decoder;
  assert(decoder.open(encoded.data(), encoded.size()) ==
         easy_input::ImaAdpcmDecoderStatus::Ok);
  assert(decoder.info().sample_rate == 48000U);
  assert(decoder.info().channels == 1U);
  assert(decoder.info().total_samples == asset.total_samples);

  std::array<std::int16_t, 480> pcm{};
  std::size_t samples = 0;
  std::uint32_t decoded = 0;
  while (decoder.decode_next(pcm.data(), pcm.size(), &samples) ==
         easy_input::ImaAdpcmDecoderStatus::Ok) {
    decoded += static_cast<std::uint32_t>(samples);
  }
  assert(decoded == asset.total_samples);
}

void assert_pcm16le(const PcmAsset& asset) {
  const auto payload = read_asset(asset.filename);
  assert(payload.size() == asset.total_samples * sizeof(std::int16_t));
  assert(payload.size() == 76800U);

  std::int16_t first = 0;
  std::int16_t last = 0;
  for (std::uint32_t index = 0U; index < asset.total_samples; ++index) {
    const auto offset = static_cast<std::size_t>(index) * 2U;
    const auto raw = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(payload[offset]) |
        (static_cast<std::uint16_t>(payload[offset + 1U]) << 8U));
    const auto sample = static_cast<std::int16_t>(raw);
    if (index == 0U) first = sample;
    if (index + 1U == asset.total_samples) last = sample;
  }
  assert(first != 0 || last != 0);
}

}  // namespace

int main() {
  for (const auto& asset : kEiadAssets) assert_eiad(asset);
  for (const auto& asset : kPcmAssets) assert_pcm16le(asset);
}
