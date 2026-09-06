#include <array>
#include <cstddef>
#include <cstdint>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard/audio_io_arbiter.h"
#include "keyboard/input_feedback.h"
#include "abo_p1/voice_engine.h"
#include "platform/gpio_keys.h"
#include "platform/led_strip_status.h"
#include "platform/peripheral_power.h"
#include "platform/instrument_voice_session.h"
#include "platform/speaker_output.h"
#include "platform/performance_runtime.h"
#include "speaker_assets/abo_p1_piano_sound.h"
#include "speaker_assets/abo_p1_sound_bank.h"
#include "abo_host/host_link.h"

namespace {

constexpr const char* kTag = "abo_app";
constexpr std::uint32_t kMainLoopPeriodMs = 10U;

std::uint32_t millis() {
  return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

struct AppContext {
  TaskHandle_t platform_task = nullptr;
  easy_input::GpioInputScanner inputs;
  easy_input::PeripheralPowerController power;
  easy_input::StatusLedStrip leds;
  ai_keyboard::AudioIoArbiter audio;
  easy_input::SpeakerOutput speaker;
  easy_input::InstrumentVoiceSession instrument_voice;
  abo_host::HostLink host;
  easy_input::PerformanceRuntime performance;
  bool speaker_power_hold_active = false;
  bool voice_session_initialized = false;
  bool voice_stream_started = false;

  AppContext() : performance(host, instrument_voice, leds) {}
};

bool is_key(ai_keyboard::InputId input) {
  const auto value = static_cast<std::size_t>(input);
  return value < static_cast<std::size_t>(ai_keyboard::InputId::Key8) + 1U;
}

const char* host_control_name(ai_keyboard::InputId input) {
  switch (input) {
    case ai_keyboard::InputId::Key1: return "s1";
    case ai_keyboard::InputId::Key2: return "s2";
    case ai_keyboard::InputId::Key3: return "s3";
    case ai_keyboard::InputId::Key4: return "s4";
    case ai_keyboard::InputId::Key5: return "s5";
    case ai_keyboard::InputId::Key6: return "s6";
    case ai_keyboard::InputId::Key7: return "s7";
    case ai_keyboard::InputId::Key8: return "s8";
    case ai_keyboard::InputId::EncoderPress: return "s9";
    case ai_keyboard::InputId::EncoderLeft:
    case ai_keyboard::InputId::EncoderRight:
    case ai_keyboard::InputId::Count:
      return nullptr;
  }
  return nullptr;
}

void handle_encoder_event(AppContext* app,
                          const easy_input::InputEvent& event) {
  ESP_LOGD(kTag, "encoder input=%u step=%d",
           static_cast<unsigned>(event.input), event.encoder_step);
  if (event.input == ai_keyboard::InputId::EncoderLeft ||
      event.input == ai_keyboard::InputId::EncoderRight) {
    app->performance.on_encoder_steps(event.encoder_step, event.timestamp_ms);
  } else if (event.input == ai_keyboard::InputId::EncoderPress &&
             event.phase == ai_keyboard::InputPhase::Pressed) {
    app->performance.on_encoder_press(event.timestamp_ms);
  }
}

bool handle_mode_request(abo_p2::PerformanceMode mode, void* context) {
  auto* app = static_cast<AppContext*>(context);
  return app->performance.set_mode(mode, millis());
}

bool handle_volume_request(std::uint8_t volume, void* context) {
  auto* app = static_cast<AppContext*>(context);
  return app->performance.set_volume(volume, millis());
}

bool handle_transport_reset(void* context) {
  auto* app = static_cast<AppContext*>(context);
  return app->performance.reset_from_transport(millis());
}

bool handle_input_event(const easy_input::InputEvent& event, void* context) {
  auto* app = static_cast<AppContext*>(context);
  const auto* control = host_control_name(event.input);
  if (control != nullptr) {
    app->host.send_input(
        control,
        event.phase == ai_keyboard::InputPhase::Pressed ? "pressed"
                                                         : "released");
  }
  app->leds.show_input_event(event.input, event.phase, event.timestamp_ms);

  if (is_key(event.input)) {
    const auto key_index = static_cast<std::uint8_t>(event.input);
    app->host.send_key(
        key_index, event.phase == ai_keyboard::InputPhase::Pressed);
    if (app->voice_stream_started) {
      const bool pressed = event.phase == ai_keyboard::InputPhase::Pressed;
      app->performance.on_key(key_index, pressed, event.timestamp_ms);
    }
    return true;
  }

  handle_encoder_event(app, event);
  return true;
}

void service_voice_session(AppContext* app) {
  app->instrument_voice.poll_bank_swap();

  if (!app->voice_session_initialized && !app->speaker.busy()) {
    const auto* piano_bank = easy_input::speaker_assets::abo_p1_sound_bank(
        easy_input::speaker_assets::kAboP1PianoInstrument);
    if (piano_bank == nullptr) {
      ESP_LOGE(kTag, "piano sound bank unavailable");
      app->voice_session_initialized = true;
      return;
    }
    const auto result = app->instrument_voice.begin(*piano_bank);
    app->voice_session_initialized = true;
    if (result != ESP_OK) {
      ESP_LOGE(kTag, "instrument voice preload failed: %s",
               esp_err_to_name(result));
    }
  }

  if (app->voice_session_initialized &&
      app->instrument_voice.ready() && !app->voice_stream_started &&
      !app->speaker.busy()) {
    if (!app->speaker.request_voice_stream(
            easy_input::instrument_voice_frame_provider,
            &app->instrument_voice)) {
      ESP_LOGW(kTag, "instrument voice stream request rejected");
    } else {
      app->voice_stream_started = true;
    }
  }
}

void service_speaker_power(AppContext* app) {
  const bool power_required = app->speaker.power_lease_required();
  if (power_required != app->speaker_power_hold_active) {
    const auto result = app->power.set_speaker_power_hold(power_required);
    if (result != ESP_OK) {
      ESP_LOGE(kTag, "speaker power hold update failed: %s",
               esp_err_to_name(result));
      return;
    }
    app->speaker_power_hold_active = power_required;
  }

  if (power_required && app->power.ready()) {
    app->speaker.notify_power_ready();
  }
  if (!app->speaker.complete_power_handoff()) {
    ESP_LOGW(kTag, "speaker ownership handoff deferred");
  }
}

void start_boot_sound(AppContext* app) {
  const auto sound = easy_input::speaker_assets::abo_p1_piano_c4_sound();
  app->speaker.mark_boot_pending(app->audio.microphone_generation());
  const auto begin_result =
      app->speaker.begin(app->platform_task, &app->audio);
  if (begin_result != ESP_OK) {
    ESP_LOGE(kTag, "speaker begin failed: %s", esp_err_to_name(begin_result));
    return;
  }

  // Boot feedback is intentionally one-shot. Sustained playback and note
  // cancellation will be enabled by the P1 key/audio task, not by this gate.
  if (!app->speaker.request_embedded_asset(
          sound.encoded, sound.encoded_bytes)) {
    ESP_LOGE(kTag, "piano C4 boot asset request failed");
  }
}

}  // namespace

extern "C" void app_main(void) {
  static AppContext app;
  app.platform_task = xTaskGetCurrentTaskHandle();

  ESP_ERROR_CHECK(app.power.begin_awake());
  app.inputs.set_notify_task(app.platform_task);
  ESP_ERROR_CHECK(app.inputs.begin(millis()));
  ESP_ERROR_CHECK(app.leds.begin());

  app.host.set_mode_request_handler(handle_mode_request, &app);
  app.host.set_volume_request_handler(handle_volume_request, &app);
  app.host.set_transport_reset_handler(handle_transport_reset, &app);
  if (app.host.begin() != ESP_OK) {
    ESP_LOGE(kTag, "USB-Serial/JTAG host link unavailable");
  } else {
    app.host.send_hello();
    if (!app.performance.begin()) {
      ESP_LOGE(kTag, "P2 performance runtime initialization failed");
    }
  }

  app.leds.reserve_cold_boot_sequence();
  start_boot_sound(&app);
  app.leds.start_cold_boot_sequence(millis());

  ESP_LOGI(kTag,
           "A Band of One independent runtime ready keys=%u usb=USB-Serial/JTAG",
           static_cast<unsigned>(ai_keyboard::kKeyPins.size()));

  while (true) {
    const auto now = millis();
    app.host.poll();
    app.inputs.poll(now, handle_input_event, &app);
    app.speaker.poll(true);
    if (app.voice_stream_started) {
      app.performance.advance_audio_frames(
          app.instrument_voice.take_rendered_frames(), now);
    }
    service_voice_session(&app);
    service_speaker_power(&app);
    app.performance.poll(now);
    app.leds.update(now);
    vTaskDelay(pdMS_TO_TICKS(kMainLoopPeriodMs));
  }
}
