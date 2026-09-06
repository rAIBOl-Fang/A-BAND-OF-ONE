if(ABO_PRODUCT)
  set(easy_input_main_sources
    "abo_app_main.cpp"
    "../abo_p1/voice_command_queue.cpp"
    "../abo_p1/voice_engine.cpp"
    "../abo_p1/mono_voice_mixer.cpp"
    "../abo_p1/voice_renderer.cpp"
    "../abo_p1/performance_controls.cpp"
    "../abo_p2/performance_controller.cpp"
    "../abo_p2/performance_clock.cpp"
    "../abo_p2/rhythm_led_model.cpp"
    "../abo_p2/score_snapshot.cpp"
    "../abo_host/host_link.cpp"
    "../abo_host/score_protocol.cpp"
    "../abo_host/wire_messages.cpp"
    "platform/gpio_keys.cpp"
    "platform/led_strip_status.cpp"
    "platform/peripheral_power.cpp"
    "platform/instrument_voice_session.cpp"
    "platform/performance_runtime.cpp"
    "platform/speaker_output.cpp"
  )

  set(easy_input_main_requires
    keyboard
    esp_driver_gpio
    esp_driver_i2s
    esp_driver_rmt
    esp_driver_usb_serial_jtag
    esp_timer
    json
    speaker_assets
  )

  idf_component_register(
    SRCS
      ${easy_input_main_sources}
    INCLUDE_DIRS
      "."
    REQUIRES
      ${easy_input_main_requires}
  )

  target_compile_definitions(${COMPONENT_LIB}
    PRIVATE
      EASY_INPUT_BOARD_V2=1
      EASY_INPUT_SPEAKER_ASSETS_PRODUCT=1
  )
  target_include_directories(${COMPONENT_LIB}
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/../abo_host/include"
      "${IDF_PATH}/components/json/cJSON"
      "${IDF_PATH}/components/esp_driver_usb_serial_jtag/include"
  )
else()
set(easy_input_main_sources
  "app_main.cpp"
  "../abo_p1/key_note_trigger.cpp"
  "platform/battery_adc.cpp"
  "platform/ble_hid.cpp"
  "platform/gpio_keys.cpp"
  "platform/keyboard_audio.cpp"
  "platform/led_strip_status.cpp"
  "platform/nvs_store.cpp"
  "platform/peripheral_power.cpp"
  "platform/usb_hid.cpp"
)

set(easy_input_main_requires
  keyboard
  bt
  esp_hid
  esp_adc
  esp_event
  esp_driver_gpio
  esp_driver_i2s
  esp_driver_rmt
  esp_netif
  esp_pm
  esp_psram
  esp_timer
  esp_wifi
  espressif__esp_tinyusb
  lwip
  nvs_flash
)

idf_build_get_property(
  easy_input_speaker_assets_product_enabled
  EASY_INPUT_SPEAKER_ASSETS_PRODUCT_ENABLED
)
idf_build_get_property(abo_p1_piano_probe_enabled ABO_P1_PIANO_PROBE_ENABLED)
idf_build_get_property(abo_p1_key_note_probe_enabled ABO_P1_KEY_NOTE_PROBE_ENABLED)

if(EASY_INPUT_SPEAKER_DIAGNOSTIC OR
   easy_input_speaker_assets_product_enabled)
  list(APPEND easy_input_main_sources "platform/speaker_output.cpp")
endif()
if(easy_input_speaker_assets_product_enabled)
  list(APPEND easy_input_main_sources
    "platform/speaker_assets_supervisor.cpp"
    "platform/speaker_assets_wifi.cpp")
endif()

idf_build_get_property(
  easy_input_speaker_opus_diagnostic_enabled
  EASY_INPUT_SPEAKER_OPUS_DIAGNOSTIC_ENABLED
)
if(easy_input_speaker_opus_diagnostic_enabled)
  list(APPEND easy_input_main_requires speaker_opus_probe)
endif()

idf_build_get_property(
  easy_input_speaker_ima_adpcm_diagnostic_enabled
  EASY_INPUT_SPEAKER_IMA_ADPCM_DIAGNOSTIC_ENABLED
)
if(easy_input_speaker_ima_adpcm_diagnostic_enabled)
  list(APPEND easy_input_main_requires speaker_ima_adpcm_probe)
endif()

idf_build_get_property(
  easy_input_speaker_assets_diagnostic_enabled
  EASY_INPUT_SPEAKER_ASSETS_DIAGNOSTIC_ENABLED
)
if(easy_input_speaker_assets_diagnostic_enabled)
  list(APPEND easy_input_main_requires speaker_assets)
endif()
if(easy_input_speaker_assets_product_enabled AND
   NOT easy_input_speaker_assets_diagnostic_enabled)
  list(APPEND easy_input_main_requires speaker_assets)
endif()

idf_component_register(
  SRCS
    ${easy_input_main_sources}
  INCLUDE_DIRS
    "."
  REQUIRES
    ${easy_input_main_requires}
)

if(abo_p1_piano_probe_enabled)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE EASY_INPUT_ABO_P1_PIANO_PROBE=1)
endif()
if(abo_p1_key_note_probe_enabled)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE EASY_INPUT_ABO_P1_KEY_NOTE_PROBE=1)
endif()
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include")

if(EASY_INPUT_BOARD STREQUAL "v2")
  target_compile_definitions(${COMPONENT_LIB} PRIVATE EASY_INPUT_BOARD_V2=1)
endif()

if(EASY_INPUT_SPEAKER_DIAGNOSTIC)
  target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE EASY_INPUT_SPEAKER_DIAGNOSTIC=1
  )
endif()

if(EASY_INPUT_SPEAKER_OPUS_DIAGNOSTIC)
  target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE EASY_INPUT_SPEAKER_OPUS_DIAGNOSTIC=1
  )
endif()

if(easy_input_speaker_ima_adpcm_diagnostic_enabled)
  target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE EASY_INPUT_SPEAKER_IMA_ADPCM_DIAGNOSTIC=1
  )
endif()

if(easy_input_speaker_assets_diagnostic_enabled)
  target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE EASY_INPUT_SPEAKER_ASSETS_DIAGNOSTIC=1
  )
endif()

if(easy_input_speaker_assets_product_enabled)
  target_compile_definitions(
    ${COMPONENT_LIB}
    PRIVATE EASY_INPUT_SPEAKER_ASSETS_PRODUCT=1
  )
endif()
endif()
