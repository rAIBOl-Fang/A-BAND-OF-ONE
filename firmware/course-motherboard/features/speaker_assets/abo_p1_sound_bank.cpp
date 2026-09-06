#include "speaker_assets/abo_p1_sound_bank.h"

namespace easy_input::speaker_assets {
namespace {

#if defined(ABO_P1_SOUND_BANK_HOST_TEST)
#define ABO_DECLARE_EMBEDDED_ASSET(prefix, symbol) \
  static const std::uint8_t prefix[] = {0U};       \
  static const std::uint8_t symbol[] = {0U}
#define ABO_EMBEDDED_ASSET_BYTES(prefix, symbol, expected) (expected)
#else
#define ABO_DECLARE_EMBEDDED_ASSET(prefix, symbol) \
  extern const std::uint8_t prefix[] asm("_binary_" #symbol "_start"); \
  extern const std::uint8_t prefix##End[] asm("_binary_" #symbol "_end")
#define ABO_EMBEDDED_ASSET_BYTES(prefix, symbol, expected) \
  static_cast<std::size_t>(prefix##End - prefix)
#endif

ABO_DECLARE_EMBEDDED_ASSET(kPianoB2, piano_b2_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoFis3, piano_fis3_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoC4, piano_c4_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoFis4, piano_fis4_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoC5, piano_c5_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoFis5, piano_fis5_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kPianoC6, piano_c6_eiad);

ABO_DECLARE_EMBEDDED_ASSET(kViolinG3, violin_g3_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinC4, violin_c4_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinE4, violin_e4_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinG4, violin_g4_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinC5, violin_c5_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinE5, violin_e5_eiad);
ABO_DECLARE_EMBEDDED_ASSET(kViolinA5, violin_a5_eiad);

ABO_DECLARE_EMBEDDED_ASSET(kClarinetD3, clarinet_d3_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetF3, clarinet_f3_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetAs3, clarinet_as3_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetD4, clarinet_d4_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetF4, clarinet_f4_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetAs4, clarinet_as4_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetD5, clarinet_d5_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetF5, clarinet_f5_pcm16le);
ABO_DECLARE_EMBEDDED_ASSET(kClarinetAs5, clarinet_as5_pcm16le);

AboP1EncodedRoot root(const char* id,
                      AboP1RootStorage storage,
                      std::uint8_t root_midi,
                      const std::uint8_t* payload,
                      std::size_t payload_bytes,
                      std::uint32_t decoded_samples,
                      std::uint32_t loop_start_sample,
                      std::uint32_t loop_end_sample) {
  return {id, storage, root_midi, payload, payload_bytes, decoded_samples,
          loop_start_sample, loop_end_sample};
}

const AboP1EncodedRoot kPianoRoots[] = {
    root("piano_b2", AboP1RootStorage::EiadV1, 47U, kPianoB2,
         ABO_EMBEDDED_ASSET_BYTES(kPianoB2, piano_b2_eiad, 49220U), 96000U,
         36000U, 86400U),
    root("piano_fis3", AboP1RootStorage::EiadV1, 54U, kPianoFis3,
         ABO_EMBEDDED_ASSET_BYTES(kPianoFis3, piano_fis3_eiad, 49220U),
         96000U, 36000U, 86400U),
    root("piano_c4", AboP1RootStorage::EiadV1, 60U, kPianoC4,
         ABO_EMBEDDED_ASSET_BYTES(kPianoC4, piano_c4_eiad, 64214U), 125257U,
         63360U, 125257U),
    root("piano_fis4", AboP1RootStorage::EiadV1, 66U, kPianoFis4,
         ABO_EMBEDDED_ASSET_BYTES(kPianoFis4, piano_fis4_eiad, 49220U),
         96000U, 36000U, 86400U),
    root("piano_c5", AboP1RootStorage::EiadV1, 72U, kPianoC5,
         ABO_EMBEDDED_ASSET_BYTES(kPianoC5, piano_c5_eiad, 49220U), 96000U,
         36000U, 86400U),
    root("piano_fis5", AboP1RootStorage::EiadV1, 78U, kPianoFis5,
         ABO_EMBEDDED_ASSET_BYTES(kPianoFis5, piano_fis5_eiad, 49220U),
         96000U, 36000U, 86400U),
    root("piano_c6", AboP1RootStorage::EiadV1, 84U, kPianoC6,
         ABO_EMBEDDED_ASSET_BYTES(kPianoC6, piano_c6_eiad, 49220U), 96000U,
         36000U, 86400U),
};

const AboP1EncodedRoot kViolinRoots[] = {
    root("violin_g3", AboP1RootStorage::EiadV1, 55U, kViolinG3,
         ABO_EMBEDDED_ASSET_BYTES(kViolinG3, violin_g3_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_c4", AboP1RootStorage::EiadV1, 60U, kViolinC4,
         ABO_EMBEDDED_ASSET_BYTES(kViolinC4, violin_c4_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_e4", AboP1RootStorage::EiadV1, 64U, kViolinE4,
         ABO_EMBEDDED_ASSET_BYTES(kViolinE4, violin_e4_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_g4", AboP1RootStorage::EiadV1, 67U, kViolinG4,
         ABO_EMBEDDED_ASSET_BYTES(kViolinG4, violin_g4_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_c5", AboP1RootStorage::EiadV1, 72U, kViolinC5,
         ABO_EMBEDDED_ASSET_BYTES(kViolinC5, violin_c5_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_e5", AboP1RootStorage::EiadV1, 76U, kViolinE5,
         ABO_EMBEDDED_ASSET_BYTES(kViolinE5, violin_e5_eiad, 36920U),
         72000U, 33600U, 62400U),
    root("violin_a5", AboP1RootStorage::EiadV1, 81U, kViolinA5,
         ABO_EMBEDDED_ASSET_BYTES(kViolinA5, violin_a5_eiad, 36920U),
         72000U, 33600U, 62400U),
};

const AboP1EncodedRoot kClarinetRoots[] = {
    root("clarinet_d3", AboP1RootStorage::Pcm16Le, 50U, kClarinetD3,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetD3, clarinet_d3_pcm16le, 76800U),
         38400U, 20160U, 27984U),
    root("clarinet_f3", AboP1RootStorage::Pcm16Le, 53U, kClarinetF3,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetF3, clarinet_f3_pcm16le, 76800U),
         38400U, 20160U, 36912U),
    root("clarinet_as3", AboP1RootStorage::Pcm16Le, 58U, kClarinetAs3,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetAs3, clarinet_as3_pcm16le, 76800U),
         38400U, 20160U, 27984U),
    root("clarinet_d4", AboP1RootStorage::Pcm16Le, 62U, kClarinetD4,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetD4, clarinet_d4_pcm16le, 76800U),
         38400U, 20160U, 32400U),
    root("clarinet_f4", AboP1RootStorage::Pcm16Le, 65U, kClarinetF4,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetF4, clarinet_f4_pcm16le, 76800U),
         38400U, 20160U, 29472U),
    root("clarinet_as4", AboP1RootStorage::Pcm16Le, 70U, kClarinetAs4,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetAs4, clarinet_as4_pcm16le, 76800U),
         38400U, 20160U, 31056U),
    root("clarinet_d5", AboP1RootStorage::Pcm16Le, 74U, kClarinetD5,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetD5, clarinet_d5_pcm16le, 76800U),
         38400U, 20160U, 29376U),
    root("clarinet_f5", AboP1RootStorage::Pcm16Le, 77U, kClarinetF5,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetF5, clarinet_f5_pcm16le, 76800U),
         38400U, 20160U, 30432U),
    root("clarinet_as5", AboP1RootStorage::Pcm16Le, 82U, kClarinetAs5,
         ABO_EMBEDDED_ASSET_BYTES(kClarinetAs5, clarinet_as5_pcm16le, 76800U),
         38400U, 20160U, 27552U),
};

const AboP1EncodedBank kBanks[] = {
    {kAboP1PianoInstrument, kPianoRoots, 7U, 701257U},
    {kAboP1ViolinInstrument, kViolinRoots, 7U, 504000U},
    {kAboP1ClarinetInstrument, kClarinetRoots, 9U, 345600U},
};

}  // namespace

const AboP1EncodedBank* abo_p1_sound_bank(std::uint8_t instrument_index) {
  if (instrument_index >= 3U) return nullptr;
  return &kBanks[instrument_index];
}

AboP1PianoSound abo_p1_piano_c4_sound() {
  const auto& c4 = kPianoRoots[2];
  return {c4.payload, c4.payload_bytes, 132U, 261U};
}

#undef ABO_DECLARE_EMBEDDED_ASSET
#undef ABO_EMBEDDED_ASSET_BYTES

}  // namespace easy_input::speaker_assets
