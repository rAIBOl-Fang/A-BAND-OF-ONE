#include <cassert>

#include "abo_p1/key_note_trigger.h"

int main() {
  abo_p1::KeyNoteTrigger trigger;

  assert(trigger.on_input(ai_keyboard::InputId::Key7,
                          ai_keyboard::InputPhase::Pressed) ==
         abo_p1::KeyNoteTriggerAction::None);
  assert(trigger.on_input(ai_keyboard::InputId::Key8,
                          ai_keyboard::InputPhase::Pressed) ==
         abo_p1::KeyNoteTriggerAction::RequestPianoC4);
  assert(trigger.active());
  assert(trigger.on_input(ai_keyboard::InputId::Key8,
                          ai_keyboard::InputPhase::Released) ==
         abo_p1::KeyNoteTriggerAction::CancelPlayback);
  assert(!trigger.active());
  assert(trigger.on_input(ai_keyboard::InputId::Key8,
                          ai_keyboard::InputPhase::Released) ==
         abo_p1::KeyNoteTriggerAction::None);
}
