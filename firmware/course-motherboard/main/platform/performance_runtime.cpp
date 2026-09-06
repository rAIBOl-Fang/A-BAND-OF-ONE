#include "platform/performance_runtime.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

#include "speaker_assets/abo_p1_sound_bank.h"

namespace easy_input {
namespace {

constexpr std::array<abo_p2::ScoreNote, 7> kDefaultNotes{{
    {0U, 96U}, {1U, 96U}, {2U, 96U}, {3U, 96U},
    {4U, 96U}, {5U, 96U}, {6U, 96U},
}};

constexpr std::string_view kDefaultScoreId = "p2-scale";

abo_p2::RhythmLedPhase to_led_phase(abo_p2::PerformancePhase phase) {
  switch (phase) {
    case abo_p2::PerformancePhase::Standby:
      return abo_p2::RhythmLedPhase::Standby;
    case abo_p2::PerformancePhase::Playing:
      return abo_p2::RhythmLedPhase::Playing;
    case abo_p2::PerformancePhase::Finished:
      return abo_p2::RhythmLedPhase::Finished;
  }
  return abo_p2::RhythmLedPhase::Standby;
}

}  // namespace

PerformanceRuntime::PerformanceRuntime(abo_host::HostLink& host,
                                       InstrumentVoiceSession& voice,
                                       StatusLedStrip& leds)
    : host_(host),
      voice_(voice),
      leds_(leds),
      controller_(abo_p2::ScoreView{kDefaultScoreId.data(), 60U,
                                    kDefaultNotes.data(), kDefaultNotes.size()}) {}

bool PerformanceRuntime::begin() {
  if (!active_score_.assign(kDefaultScoreId, 60U, kDefaultNotes.data(),
                            kDefaultNotes.size())) {
    return false;
  }
  if (!controller_.set_score(active_score_.view())) return false;
  active_score_crc32_ = 0U;
  started_ = true;
  refresh_score_at_standby();
  publish_state(0U, true);
  return true;
}

bool PerformanceRuntime::set_mode(abo_p2::PerformanceMode mode,
                                  std::uint32_t now_ms) {
  if (!started_ || !controller_.set_mode(mode)) return false;
  execute_actions(now_ms);
  return true;
}

bool PerformanceRuntime::set_volume(std::uint8_t volume,
                                    std::uint32_t now_ms) {
  if (!started_ ||
      controller_.state().phase == abo_p2::PerformancePhase::Finished) {
    return false;
  }
  if (!voice_.set_volume(volume)) return false;
  controller_.set_volume(volume);
  publish_state(now_ms, true);
  return true;
}

bool PerformanceRuntime::reset_from_transport(std::uint32_t now_ms) {
  if (!started_ || !controller_.reset_from_finished()) return false;
  sync_led_phase(now_ms);
  render_leds(now_ms);
  // HostLink sends the ACK first and the refreshed state second. Stage the
  // cache here without emitting an early state, so transport reset preserves
  // that wire ordering.
  publish_state(now_ms, true, false);
  return true;
}

void PerformanceRuntime::on_key(std::uint8_t key_index,
                                bool pressed,
                                std::uint32_t now_ms) {
  if (!started_) return;
  if (pressed) {
    controller_.key_down(key_index);
  } else {
    controller_.key_up(key_index);
  }
  execute_actions(now_ms);
}

void PerformanceRuntime::on_encoder_steps(int steps, std::uint32_t now_ms) {
  if (!started_) return;
  controller_.encoder_steps(steps);
  execute_actions(now_ms);
}

void PerformanceRuntime::on_encoder_press(std::uint32_t now_ms) {
  if (!started_) return;
  controller_.encoder_press();
  execute_actions(now_ms);
}

void PerformanceRuntime::advance_audio_frames(std::uint64_t frames,
                                              std::uint32_t now_ms) {
  if (!started_ || frames == 0U) return;
  while (frames > 0U) {
    const auto chunk = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        frames, std::numeric_limits<std::uint32_t>::max()));
    controller_.advance_frames(chunk);
    frames -= chunk;
  }
  execute_actions(now_ms);
}

void PerformanceRuntime::poll(std::uint32_t now_ms) {
  if (!started_) return;
  const bool score_changed = refresh_score_at_standby();
  const bool loading_changed =
      last_instrument_loading_ != voice_.bank_loading();
  last_instrument_loading_ = voice_.bank_loading();
  sync_led_phase(now_ms);
  render_leds(now_ms);
  publish_state(now_ms, score_changed || loading_changed);
}

bool PerformanceRuntime::refresh_score_at_standby() {
  if (controller_.state().phase != abo_p2::PerformancePhase::Standby) {
    return false;
  }

  const auto* committed = host_.score_protocol().committed_score();
  if (committed == nullptr) return false;

  const auto id = std::string_view(committed->id.data(), committed->id_length);
  const auto current = active_score_.view();
  const auto committed_crc32 = abo_host::ScoreProtocol::canonical_crc32(*committed);
  if (active_score_.valid() && active_score_.id() == id &&
      current.bpm == committed->bpm &&
      current.note_count == committed->note_count &&
      active_score_crc32_ == committed_crc32) {
    return false;
  }

  for (std::size_t index = 0U; index < committed->note_count; ++index) {
    converted_notes_[index] = abo_p2::ScoreNote{
        committed->notes[index].n, committed->notes[index].ticks};
  }
  if (!active_score_.assign(id, committed->bpm, converted_notes_.data(),
                            committed->note_count)) {
    host_.send_runtime_error("score_snapshot_failed");
    return false;
  }
  if (!controller_.set_score(active_score_.view())) {
    host_.send_runtime_error("score_not_standby");
    return false;
  }
  active_score_crc32_ = committed_crc32;
  return true;
}

void PerformanceRuntime::execute_actions(std::uint32_t now_ms) {
  std::array<abo_p2::PerformanceAction,
             abo_p2::PerformanceController::kActionCapacity>
      actions{};
  const auto count = controller_.drain_actions(actions.data(), actions.size());
  for (std::size_t index = 0U; index < count; ++index) {
    const auto& action = actions[index];
    switch (action.type) {
      case abo_p2::PerformanceActionType::NoteOn: {
        if (action.key_index >= note_generations_.size()) break;
        const auto generation = ++next_generation_;
        note_generations_[action.key_index] = generation;
        const auto ok = voice_.note_on(
            action.key_index, generation,
            static_cast<abo_p1::Instrument>(controller_.state().instrument),
            action.key_index, static_cast<std::int8_t>(controller_.state().octave));
        rhythm_leds_.on_beat_tick(
            static_cast<std::uint8_t>(controller_.state().cursor % 5U), now_ms);
        if (!ok) fail_platform("note_on_failed", now_ms);
        break;
      }
      case abo_p2::PerformanceActionType::NoteOff: {
        if (action.key_index >= note_generations_.size()) break;
        if (!voice_.note_off(action.key_index,
                             note_generations_[action.key_index])) {
          fail_platform("note_off_failed", now_ms);
        }
        break;
      }
      case abo_p2::PerformanceActionType::LoadInstrument:
        if (!request_instrument(action.instrument)) {
          controller_.restore_instrument(action.previous_instrument);
          fail_platform("instrument_load_rejected", now_ms);
        }
        break;
      case abo_p2::PerformanceActionType::Judge:
        if (action.ok) {
          rhythm_leds_.on_correct(now_ms);
        } else {
          rhythm_leds_.on_error(now_ms);
        }
        host_.send_judge(action.ok, action.key_index, action.expected_index,
                         controller_.state().errors);
        break;
      case abo_p2::PerformanceActionType::Result:
        host_.send_result(controller_.state().errors, elapsed_ms());
        break;
      case abo_p2::PerformanceActionType::StateChanged:
        sync_led_phase(now_ms);
        publish_state(now_ms, true);
        break;
      case abo_p2::PerformanceActionType::LedEvent:
      case abo_p2::PerformanceActionType::None:
        break;
    }
  }
  sync_led_phase(now_ms);
  render_leds(now_ms);
  publish_state(now_ms, true);
}

void PerformanceRuntime::publish_state(std::uint32_t now_ms,
                                        bool force,
                                        bool send) {
  if (!started_ ||
      (!force && state_published_ && now_ms - last_state_publish_ms_ < 100U)) {
    return;
  }
  const auto frame = rhythm_leds_.frame_at(now_ms);
  std::array<bool, 5> led_bits{};
  for (std::size_t index = 0U; index < led_bits.size(); ++index) {
    const auto& pixel = frame.pixels[index];
    led_bits[index] = pixel.red != 0U || pixel.green != 0U || pixel.blue != 0U;
  }
  const auto view = active_score_.view();
  if (controller_.state().mode == abo_p2::PerformanceMode::Score) {
    std::array<const char*, 8> score_keys{{
        "idle", "idle", "idle", "idle", "idle", "idle", "idle", "idle"}};
    if (controller_.state().phase == abo_p2::PerformancePhase::Playing) {
      if (controller_.state().subphase == abo_p2::PerformanceSubphase::Waiting &&
          controller_.state().cursor < view.note_count) {
        score_keys[view.notes[controller_.state().cursor].solfege] = "waiting";
      } else if (controller_.state().active_key < score_keys.size()) {
        score_keys[controller_.state().active_key] = "holding";
      }
    }
    host_.set_key_states(score_keys);
  }
  const auto& performance_state = controller_.state();
  const bool holding =
      performance_state.phase == abo_p2::PerformancePhase::Playing &&
      performance_state.subphase == abo_p2::PerformanceSubphase::Holding &&
      performance_state.cursor < view.note_count;
  const auto note_ticks = holding ? performance_state.score_ticks : 0U;
  const auto note_total_ticks =
      holding ? static_cast<std::uint32_t>(
                    view.notes[performance_state.cursor].ticks)
              : 0U;
  host_.set_p2_state(performance_state, active_score_.id(),
                     view.note_count != 0U, led_bits, voice_.bank_loading(),
                     static_cast<std::int16_t>(
                         (static_cast<int>(performance_state.octave) - 4) * 45),
                     elapsed_ms(), active_score_crc32_, note_ticks,
                     note_total_ticks);
  if (send) host_.send_state();
  state_published_ = true;
  last_state_publish_ms_ = now_ms;
}

void PerformanceRuntime::render_leds(std::uint32_t now_ms) {
  const auto frame = rhythm_leds_.frame_at(now_ms);
  std::array<Rgb, 5> physical{};
  for (std::size_t index = 0U; index < physical.size(); ++index) {
    physical[index] = Rgb{frame.pixels[index].red, frame.pixels[index].green,
                          frame.pixels[index].blue};
  }
  leds_.show_rhythm_frame(physical);
}

void PerformanceRuntime::sync_led_phase(std::uint32_t now_ms) {
  const auto phase = controller_.state().phase;
  if (phase == rendered_phase_) return;
  rendered_phase_ = phase;
  rhythm_leds_.set_phase(to_led_phase(phase), now_ms);
}

void PerformanceRuntime::fail_platform(const char* reason,
                                       std::uint32_t now_ms) {
  host_.send_runtime_error(reason);
  controller_.platform_failure();
  sync_led_phase(now_ms);
}

bool PerformanceRuntime::request_instrument(std::uint8_t instrument) {
  if (instrument >= kInstrumentCount) return false;
  const auto* bank = speaker_assets::abo_p1_sound_bank(instrument);
  return bank != nullptr && voice_.request_bank(bank);
}

std::uint32_t PerformanceRuntime::elapsed_ms() const {
  const auto frames = controller_.state().elapsed_frames / 48U;
  return frames > std::numeric_limits<std::uint32_t>::max()
             ? std::numeric_limits<std::uint32_t>::max()
             : static_cast<std::uint32_t>(frames);
}

}  // namespace easy_input
