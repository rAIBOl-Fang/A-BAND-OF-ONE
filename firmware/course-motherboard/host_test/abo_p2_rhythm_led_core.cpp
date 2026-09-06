// Keep the host target independent from the ESP-IDF platform build. The
// production implementation is compiled into this test through a wrapper so
// the project path never becomes a compiler source path.
#include "../abo_p2/rhythm_led_model.cpp"
