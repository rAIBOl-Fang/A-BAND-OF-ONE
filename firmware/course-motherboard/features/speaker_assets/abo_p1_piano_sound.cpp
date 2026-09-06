#include "speaker_assets/abo_p1_piano_sound.h"

namespace easy_input::speaker_assets {
namespace {

extern const std::uint8_t kAboP1PianoC4Start[] asm("_binary_piano_c4_eiad_start");
extern const std::uint8_t kAboP1PianoC4End[] asm("_binary_piano_c4_eiad_end");

}  // namespace

AboP1PianoSound abo_p1_piano_c4_sound() {
  return {kAboP1PianoC4Start,
          static_cast<std::size_t>(kAboP1PianoC4End - kAboP1PianoC4Start),
          132U,
          261U};
}

}  // namespace easy_input::speaker_assets
