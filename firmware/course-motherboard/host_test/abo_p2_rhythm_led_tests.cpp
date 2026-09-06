#include <array>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

#include "abo_p2/rhythm_led_model.h"

namespace {

using abo_p2::RhythmLedColor;
using abo_p2::RhythmLedFrame;
using abo_p2::RhythmLedModel;
using abo_p2::RhythmLedPhase;

bool equal_color(const RhythmLedColor& left, const RhythmLedColor& right) {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

bool all_equal(const RhythmLedFrame& frame, RhythmLedColor color) {
  for (const auto& pixel : frame.pixels) {
    if (!equal_color(pixel, color)) {
      return false;
    }
  }
  return true;
}

bool all_off(const RhythmLedFrame& frame) {
  return all_equal(frame, RhythmLedColor{});
}

void playing_moves_one_blue_beat_point_across_five_leds() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Playing, 0U);

  model.on_beat_tick(0U, 0U);
  auto frame = model.frame_at(0U);
  assert(equal_color(frame.pixels[0], RhythmLedModel::kBlue));
  for (std::size_t index = 1U; index < frame.pixels.size(); ++index) {
    assert(!equal_color(frame.pixels[index], RhythmLedModel::kBlue));
  }

  model.on_beat_tick(4U, 1000U);
  frame = model.frame_at(1000U);
  assert(equal_color(frame.pixels[4], RhythmLedModel::kBlue));

  model.on_beat_tick(5U, 2000U);
  frame = model.frame_at(2000U);
  assert(equal_color(frame.pixels[0], RhythmLedModel::kBlue));
}

void correct_key_is_a_short_green_pulse() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Playing, 0U);
  model.on_beat_tick(2U, 100U);
  model.on_correct(100U);

  assert(all_equal(model.frame_at(100U), RhythmLedModel::kGreen));
  assert(all_equal(model.frame_at(100U + RhythmLedModel::kCorrectPulseMs - 1U),
                   RhythmLedModel::kGreen));
  assert(equal_color(model.frame_at(100U + RhythmLedModel::kCorrectPulseMs)
                         .pixels[2],
                     RhythmLedModel::kBlue));
}

void wrong_key_is_a_120_ms_red_flash() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Playing, 0U);
  model.on_beat_tick(1U, 500U);
  model.on_error(500U);

  assert(all_equal(model.frame_at(500U), RhythmLedModel::kRed));
  assert(all_equal(model.frame_at(500U + RhythmLedModel::kErrorPulseMs - 1U),
                   RhythmLedModel::kRed));
  assert(equal_color(model.frame_at(500U + RhythmLedModel::kErrorPulseMs)
                         .pixels[1],
                     RhythmLedModel::kBlue));
}

void paused_is_low_brightness_amber() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Paused, 1000U);
  assert(all_equal(model.frame_at(1000U), RhythmLedModel::kPausedAmber));
  assert(all_equal(model.frame_at(60000U), RhythmLedModel::kPausedAmber));
}

void finished_runs_one_color_round_trip_then_turns_off() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Finished, 2000U);

  const auto first = model.frame_at(2000U);
  assert(!all_off(first));
  assert(!equal_color(first.pixels[0], first.pixels[1]));

  const auto last = model.frame_at(
      2000U + RhythmLedModel::kFinishedDurationMs);
  assert(all_off(last));
  assert(all_off(model.frame_at(2000U +
                                RhythmLedModel::kFinishedDurationMs + 1U)));
}

void error_overrides_correct_and_correct_overrides_beat() {
  RhythmLedModel model;
  model.set_phase(RhythmLedPhase::Playing, 0U);
  model.on_beat_tick(3U, 100U);
  model.on_correct(100U);
  assert(all_equal(model.frame_at(100U), RhythmLedModel::kGreen));
  model.on_error(100U);
  assert(all_equal(model.frame_at(100U), RhythmLedModel::kRed));
}

void cold_boot_platform_keeps_led_ownership_until_sequence_finishes() {
  std::ifstream source("main/platform/led_strip_status.cpp");
  assert(source.good());
  const std::string text((std::istreambuf_iterator<char>(source)),
                         std::istreambuf_iterator<char>());
  const auto method_start = text.find("StatusLedStrip::show_rhythm_frame");
  const auto method_end = text.find("void StatusLedStrip::clear", method_start);
  assert(method_start != std::string::npos);
  assert(method_end != std::string::npos);
  const auto method = text.substr(method_start, method_end - method_start);
  assert(text.find("StatusLedStrip::show_rhythm_frame") != std::string::npos);
  assert(text.find("if (cold_boot_sequence_active())") != std::string::npos);
  assert(method.find("return;") != std::string::npos);
  assert(method.find("rmt_new_tx_channel") == std::string::npos);
}

}  // namespace

int main() {
  playing_moves_one_blue_beat_point_across_five_leds();
  correct_key_is_a_short_green_pulse();
  wrong_key_is_a_120_ms_red_flash();
  paused_is_low_brightness_amber();
  finished_runs_one_color_round_trip_then_turns_off();
  error_overrides_correct_and_correct_overrides_beat();
  cold_boot_platform_keeps_led_ownership_until_sequence_finishes();
  return 0;
}
