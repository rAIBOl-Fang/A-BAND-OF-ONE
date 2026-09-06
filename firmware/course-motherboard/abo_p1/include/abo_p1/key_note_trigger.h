#pragma once

#include "keyboard/keymap.h"

namespace abo_p1 {

enum class KeyNoteTriggerAction {
  None,
  RequestPianoC4,
  CancelPlayback,
};

class KeyNoteTrigger {
 public:
  KeyNoteTriggerAction on_input(ai_keyboard::InputId input,
                                ai_keyboard::InputPhase phase);
  bool active() const;

 private:
  bool active_ = false;
};

}  // namespace abo_p1
