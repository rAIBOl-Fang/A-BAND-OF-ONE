#include "abo_p1/key_note_trigger.h"

namespace abo_p1 {

KeyNoteTriggerAction KeyNoteTrigger::on_input(
    ai_keyboard::InputId input,
    ai_keyboard::InputPhase phase) {
  if (input != ai_keyboard::InputId::Key8) {
    return KeyNoteTriggerAction::None;
  }
  if (phase == ai_keyboard::InputPhase::Pressed && !active_) {
    active_ = true;
    return KeyNoteTriggerAction::RequestPianoC4;
  }
  if (phase == ai_keyboard::InputPhase::Released && active_) {
    active_ = false;
    return KeyNoteTriggerAction::CancelPlayback;
  }
  return KeyNoteTriggerAction::None;
}

bool KeyNoteTrigger::active() const {
  return active_;
}

}  // namespace abo_p1
