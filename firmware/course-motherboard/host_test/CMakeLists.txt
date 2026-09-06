cmake_minimum_required(VERSION 3.20)

project(easy_input_keyboard_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

# These host executables use <cassert> as their test oracle, including calls
# with intentional side effects inside assert(...). Release generators define
# NDEBUG globally, which would otherwise remove both the checks and the calls,
# turning the suite into a false green (and causing undefined downstream test
# state in a few cases). Keep assertions enabled for every test executable.
function(enable_keyboard_host_test_assertions target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /UNDEBUG)
  else()
    target_compile_options(${target} PRIVATE -UNDEBUG)
  endif()
endfunction()

add_library(easy_input_keyboard_core
  ../components/keyboard/src/audio_io_arbiter.cpp
  ../components/keyboard/src/audio_session.cpp
  ../components/keyboard/src/audio_control_wire.cpp
  ../components/keyboard/src/awake_wait_planner.cpp
  ../components/keyboard/src/boot_led_sequence.cpp
  ../components/keyboard/src/cold_boot_feedback.cpp
  ../components/keyboard/src/battery_estimator.cpp
  ../components/keyboard/src/ble_advertising_state.cpp
  ../components/keyboard/src/ble_fixed_text_stream.cpp
  ../components/keyboard/src/ble_input_scheduler.cpp
  ../components/keyboard/src/ble_owner_recovery.cpp
  ../components/keyboard/src/config_payload.cpp
  ../components/keyboard/src/config_receiver.cpp
  ../components/keyboard/src/config_state.cpp
  ../components/keyboard/src/config_status.cpp
  ../components/keyboard/src/debounce.cpp
  ../components/keyboard/src/diagnostic_command.cpp
  ../components/keyboard/src/encoder.cpp
  ../components/keyboard/src/encoder_press_gesture.cpp
  ../components/keyboard/src/held_keyboard_state.cpp
  ../components/keyboard/src/hid_report_queue.cpp
  ../components/keyboard/src/hid_keycode.cpp
  ../components/keyboard/src/input_feedback.cpp
  ../components/keyboard/src/input_test.cpp
  ../components/keyboard/src/keyboard_snapshot_delivery.cpp
  ../components/keyboard/src/keymap.cpp
  ../components/keyboard/src/platform_selection.cpp
  ../components/keyboard/src/peripheral_power_lease.cpp
  ../components/keyboard/src/power_cycle.cpp
  ../components/keyboard/src/power_policy.cpp
  ../components/keyboard/src/speaker_probe_status.cpp
  ../components/keyboard/src/speaker_playback.cpp
  ../components/keyboard/src/speaker_service_startup.cpp
  ../components/keyboard/src/status_hid_protocol.cpp
  ../components/keyboard/src/transport_routing.cpp
)

target_include_directories(easy_input_keyboard_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/keyboard/include
)

function(add_keyboard_host_test target)
  add_executable(${target} ${target}.cpp)
  target_link_libraries(${target} PRIVATE easy_input_keyboard_core)
  enable_keyboard_host_test_assertions(${target})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_keyboard_host_test(board_pins_tests)

add_executable(board_pins_v2_tests board_pins_tests.cpp)
target_link_libraries(board_pins_v2_tests PRIVATE easy_input_keyboard_core)
target_compile_definitions(board_pins_v2_tests PRIVATE EASY_INPUT_BOARD_V2=1)
enable_keyboard_host_test_assertions(board_pins_v2_tests)
add_test(NAME board_pins_v2_tests COMMAND board_pins_v2_tests)

add_keyboard_host_test(audio_io_arbiter_tests)
add_keyboard_host_test(audio_control_wire_tests)
add_keyboard_host_test(audio_packet_wire_tests)
add_keyboard_host_test(audio_session_tests)
add_keyboard_host_test(agent_status_tests)
add_keyboard_host_test(awake_wait_planner_tests)
add_keyboard_host_test(boot_led_sequence_tests)
add_keyboard_host_test(cold_boot_feedback_tests)
add_keyboard_host_test(battery_estimator_tests)
add_keyboard_host_test(ble_advertising_state_tests)
add_keyboard_host_test(ble_connection_profile_tests)
add_keyboard_host_test(ble_management_gate_tests)
add_keyboard_host_test(ble_input_scheduler_tests)
add_keyboard_host_test(ble_fixed_text_stream_tests)
add_keyboard_host_test(ble_owner_recovery_tests)
add_keyboard_host_test(ble_persistence_policy_tests)
add_keyboard_host_test(ble_status_wire_tests)
add_keyboard_host_test(config_payload_tests)
add_keyboard_host_test(config_receiver_tests)
add_keyboard_host_test(config_state_tests)
add_keyboard_host_test(config_status_tests)
add_keyboard_host_test(debounce_tests)
add_keyboard_host_test(diagnostic_command_tests)
add_keyboard_host_test(encoder_tests)
add_keyboard_host_test(encoder_press_gesture_tests)

add_executable(
  abo_encoder_press_dispatch_contract_tests
  abo_encoder_press_dispatch_contract_tests.cpp
)
target_compile_definitions(
  abo_encoder_press_dispatch_contract_tests
  PRIVATE EASY_INPUT_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
enable_keyboard_host_test_assertions(abo_encoder_press_dispatch_contract_tests)
add_test(
  NAME abo_encoder_press_dispatch_contract_tests
  COMMAND abo_encoder_press_dispatch_contract_tests
)
set_tests_properties(
  abo_encoder_press_dispatch_contract_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_mode_control_contract_tests
  abo_mode_control_contract_tests.cpp
)
enable_keyboard_host_test_assertions(abo_mode_control_contract_tests)
add_test(
  NAME abo_mode_control_contract_tests
  COMMAND abo_mode_control_contract_tests
)
set_tests_properties(
  abo_mode_control_contract_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_p2_performance_controller_tests
  abo_p2_performance_controller_tests.cpp
  abo_p2_performance_controller_core.cpp
)
target_include_directories(
  abo_p2_performance_controller_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include
)
enable_keyboard_host_test_assertions(abo_p2_performance_controller_tests)
add_test(
  NAME abo_p2_performance_controller_tests
  COMMAND abo_p2_performance_controller_tests
)
set_tests_properties(
  abo_p2_performance_controller_tests
  PROPERTIES
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_p2_performance_clock_tests
  abo_p2_performance_clock_tests.cpp
  abo_p2_performance_clock_core.cpp
)
target_include_directories(
  abo_p2_performance_clock_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include
)
enable_keyboard_host_test_assertions(abo_p2_performance_clock_tests)
add_test(
  NAME abo_p2_performance_clock_tests
  COMMAND abo_p2_performance_clock_tests
)
set_tests_properties(
  abo_p2_performance_clock_tests
  PROPERTIES
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_p2_rhythm_led_tests
  abo_p2_rhythm_led_tests.cpp
  abo_p2_rhythm_led_core.cpp
)
target_include_directories(
  abo_p2_rhythm_led_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include
)
enable_keyboard_host_test_assertions(abo_p2_rhythm_led_tests)
add_test(
  NAME abo_p2_rhythm_led_tests
  COMMAND abo_p2_rhythm_led_tests
)
set_tests_properties(
  abo_p2_rhythm_led_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_p2_platform_contract_tests
  abo_p2_platform_contract_tests.cpp
  abo_p2_score_snapshot_core.cpp
  abo_p2_performance_controller_core.cpp
)
target_include_directories(
  abo_p2_platform_contract_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_host/include
)
enable_keyboard_host_test_assertions(abo_p2_platform_contract_tests)
add_test(
  NAME abo_p2_platform_contract_tests
  COMMAND abo_p2_platform_contract_tests
)
set_tests_properties(
  abo_p2_platform_contract_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  firmware_source_contract_tests
  abo_independent_release_contract_tests.cpp
)
enable_keyboard_host_test_assertions(firmware_source_contract_tests)
add_test(NAME firmware_source_contract_tests COMMAND firmware_source_contract_tests)
set_tests_properties(
  firmware_source_contract_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)
add_keyboard_host_test(held_keyboard_state_tests)
add_keyboard_host_test(hid_report_queue_tests)
add_keyboard_host_test(hid_keycode_tests)
add_keyboard_host_test(input_feedback_tests)
add_keyboard_host_test(input_test_tests)

add_executable(
  speaker_assets_wifi_policy_tests
  speaker_assets_wifi_policy_tests.cpp
)
target_include_directories(
  speaker_assets_wifi_policy_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(
  speaker_assets_wifi_policy_tests)
add_test(
  NAME speaker_assets_wifi_policy_tests
  COMMAND speaker_assets_wifi_policy_tests
)

add_executable(
  ima_adpcm_decoder_tests
  ima_adpcm_decoder_tests.cpp
  ../diagnostics/speaker_ima_adpcm_probe/ima_adpcm_decoder.cpp
)
target_include_directories(
  ima_adpcm_decoder_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostics/speaker_ima_adpcm_probe
    ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostics/speaker_ima_adpcm_probe/include
)
enable_keyboard_host_test_assertions(ima_adpcm_decoder_tests)
add_test(NAME ima_adpcm_decoder_tests COMMAND ima_adpcm_decoder_tests)

add_executable(
  abo_p1_asset_decode_tests
  abo_p1_asset_decode_tests.cpp
  ../diagnostics/speaker_ima_adpcm_probe/ima_adpcm_decoder.cpp
)
target_include_directories(
  abo_p1_asset_decode_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostics/speaker_ima_adpcm_probe/include
)
enable_keyboard_host_test_assertions(abo_p1_asset_decode_tests)
add_test(NAME abo_p1_asset_decode_tests COMMAND abo_p1_asset_decode_tests)
set_tests_properties(
  abo_p1_asset_decode_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_executable(
  abo_p1_sound_bank_contract_tests
  abo_p1_sound_bank_contract_tests.cpp
  ../features/speaker_assets/abo_p1_sound_bank.cpp
)
target_include_directories(
  abo_p1_sound_bank_contract_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
target_compile_definitions(
  abo_p1_sound_bank_contract_tests
  PRIVATE ABO_P1_SOUND_BANK_HOST_TEST=1
)
enable_keyboard_host_test_assertions(abo_p1_sound_bank_contract_tests)
add_test(NAME abo_p1_sound_bank_contract_tests COMMAND abo_p1_sound_bank_contract_tests)

add_executable(
  sound_asset_store_tests
  sound_asset_store_tests.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
  ../features/speaker_assets/sound_asset_format.cpp
  ../features/speaker_assets/sound_asset_store.cpp
)
target_include_directories(
  sound_asset_store_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(sound_asset_store_tests)
add_test(NAME sound_asset_store_tests COMMAND sound_asset_store_tests)

add_executable(
  sound_asset_reader_tests
  sound_asset_reader_tests.cpp
  ../features/speaker_assets/sound_asset_reader.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
  ../features/speaker_assets/sound_asset_format.cpp
  ../features/speaker_assets/sound_asset_store.cpp
)
target_include_directories(
  sound_asset_reader_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
target_compile_definitions(
  sound_asset_reader_tests
  PRIVATE EASY_INPUT_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
enable_keyboard_host_test_assertions(sound_asset_reader_tests)
add_test(NAME sound_asset_reader_tests COMMAND sound_asset_reader_tests)

add_executable(
  speaker_assets_store_executor_tests
  speaker_assets_store_executor_tests.cpp
  ../features/speaker_assets/speaker_assets_store_executor.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
  ../features/speaker_assets/sound_asset_format.cpp
  ../features/speaker_assets/sound_asset_store.cpp
)
target_include_directories(
  speaker_assets_store_executor_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(speaker_assets_store_executor_tests)
add_test(
  NAME speaker_assets_store_executor_tests
  COMMAND speaker_assets_store_executor_tests
)

add_executable(
  speaker_assets_flash_runner_tests
  speaker_assets_flash_runner_tests.cpp
  ../features/speaker_assets/speaker_assets_flash_runner.cpp
  ../features/speaker_assets/speaker_assets_store_executor.cpp
  ../features/speaker_assets/speaker_assets_runtime.cpp
  ../features/speaker_assets/speaker_assets_session.cpp
  ../features/speaker_assets/speaker_assets_protocol.cpp
  ../features/speaker_assets/sound_asset_reader.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
  ../features/speaker_assets/sound_asset_format.cpp
  ../features/speaker_assets/sound_asset_store.cpp
)
target_include_directories(
  speaker_assets_flash_runner_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
target_compile_definitions(
  speaker_assets_flash_runner_tests
  PRIVATE EASY_INPUT_SPEAKER_ASSETS_RUNTIME_TEST_ACCESS=1
)
enable_keyboard_host_test_assertions(speaker_assets_flash_runner_tests)
add_test(
  NAME speaker_assets_flash_runner_tests
  COMMAND speaker_assets_flash_runner_tests
)

add_executable(
  speaker_assets_protocol_tests
  speaker_assets_protocol_tests.cpp
  ../features/speaker_assets/speaker_assets_protocol.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
)
target_include_directories(
  speaker_assets_protocol_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(speaker_assets_protocol_tests)
add_test(NAME speaker_assets_protocol_tests COMMAND speaker_assets_protocol_tests)

add_executable(
  speaker_assets_wifi_wire_tests
  speaker_assets_wifi_wire_tests.cpp
  ../features/speaker_assets/speaker_assets_wifi_wire.cpp
  ../features/speaker_assets/speaker_assets_protocol.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
)
target_include_directories(
  speaker_assets_wifi_wire_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(speaker_assets_wifi_wire_tests)
add_test(
  NAME speaker_assets_wifi_wire_tests
  COMMAND speaker_assets_wifi_wire_tests
)

add_executable(
  speaker_assets_session_tests
  speaker_assets_session_tests.cpp
  ../features/speaker_assets/speaker_assets_session.cpp
  ../features/speaker_assets/speaker_assets_protocol.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
)
target_include_directories(
  speaker_assets_session_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
enable_keyboard_host_test_assertions(speaker_assets_session_tests)
add_test(NAME speaker_assets_session_tests COMMAND speaker_assets_session_tests)

add_executable(
  speaker_assets_runtime_tests
  speaker_assets_runtime_tests.cpp
  ../features/speaker_assets/speaker_assets_runtime.cpp
  ../features/speaker_assets/speaker_assets_session.cpp
  ../features/speaker_assets/speaker_assets_protocol.cpp
  ../features/speaker_assets/sound_asset_crypto.cpp
)
target_include_directories(
  speaker_assets_runtime_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../features/speaker_assets/include
)
target_compile_definitions(
  speaker_assets_runtime_tests
  PRIVATE EASY_INPUT_SPEAKER_ASSETS_RUNTIME_TEST_ACCESS=1
)
enable_keyboard_host_test_assertions(speaker_assets_runtime_tests)
add_test(NAME speaker_assets_runtime_tests COMMAND speaker_assets_runtime_tests)

add_keyboard_host_test(gatt_status_snapshot_tests)
add_keyboard_host_test(host_action_protocol_tests)
target_compile_definitions(
  host_action_protocol_tests
  PRIVATE EASY_INPUT_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)
add_keyboard_host_test(keyboard_snapshot_delivery_tests)
add_keyboard_host_test(keymap_tests)
add_keyboard_host_test(platform_selection_tests)
add_keyboard_host_test(peripheral_power_lease_tests)
add_keyboard_host_test(power_cycle_tests)
add_keyboard_host_test(power_policy_tests)
add_keyboard_host_test(speaker_audio_contract_tests)
add_keyboard_host_test(speaker_probe_status_tests)
add_keyboard_host_test(speaker_playback_tests)
add_keyboard_host_test(speaker_service_startup_tests)
add_keyboard_host_test(status_hid_protocol_tests)
add_keyboard_host_test(transport_routing_tests)
add_keyboard_host_test(usb_hid_endpoint_arbiter_tests)

add_executable(
  abo_p0_startup_self_check_tests
  abo_p0_startup_self_check_tests.cpp
  ../abo_p0/startup_self_check.cpp
)
target_include_directories(
  abo_p0_startup_self_check_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p0/include
)
enable_keyboard_host_test_assertions(abo_p0_startup_self_check_tests)
add_test(NAME abo_p0_startup_self_check_tests COMMAND abo_p0_startup_self_check_tests)

add_executable(
  abo_p1_key_note_trigger_tests
  abo_p1_key_note_trigger_tests.cpp
  ../abo_p1/key_note_trigger.cpp
)
target_include_directories(
  abo_p1_key_note_trigger_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/keyboard/include
)
enable_keyboard_host_test_assertions(abo_p1_key_note_trigger_tests)
add_test(NAME abo_p1_key_note_trigger_tests COMMAND abo_p1_key_note_trigger_tests)

add_executable(
  abo_voice_engine_tests
  abo_voice_engine_tests.cpp
  abo_voice_engine_core.cpp
)
target_include_directories(
  abo_voice_engine_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
)
enable_keyboard_host_test_assertions(abo_voice_engine_tests)
add_test(NAME abo_voice_engine_tests COMMAND abo_voice_engine_tests)

add_executable(
  abo_voice_renderer_tests
  abo_voice_renderer_tests.cpp
  abo_voice_renderer_core.cpp
  abo_voice_engine_core.cpp
)
target_include_directories(
  abo_voice_renderer_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
)
enable_keyboard_host_test_assertions(abo_voice_renderer_tests)
add_test(NAME abo_voice_renderer_tests COMMAND abo_voice_renderer_tests)

add_executable(
  abo_voice_command_queue_tests
  abo_voice_command_queue_tests.cpp
  abo_voice_command_queue_core.cpp
)
target_include_directories(
  abo_voice_command_queue_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
)
enable_keyboard_host_test_assertions(abo_voice_command_queue_tests)
add_test(NAME abo_voice_command_queue_tests COMMAND abo_voice_command_queue_tests)

add_executable(
  abo_mono_voice_mixer_tests
  abo_mono_voice_mixer_tests.cpp
  abo_mono_voice_mixer_core.cpp
)
target_include_directories(
  abo_mono_voice_mixer_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
)
enable_keyboard_host_test_assertions(abo_mono_voice_mixer_tests)
add_test(NAME abo_mono_voice_mixer_tests COMMAND abo_mono_voice_mixer_tests)

add_executable(
  abo_p3_volume_red_tests
  abo_p3_volume_red_tests.cpp
)
enable_keyboard_host_test_assertions(abo_p3_volume_red_tests)
add_test(NAME abo_p3_volume_red_tests COMMAND abo_p3_volume_red_tests)
set_tests_properties(
  abo_p3_volume_red_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)

add_executable(
  abo_instrument_voice_contract_tests
  abo_instrument_voice_contract_tests.cpp
)
enable_keyboard_host_test_assertions(abo_instrument_voice_contract_tests)
add_test(
  NAME abo_instrument_voice_contract_tests
  COMMAND abo_instrument_voice_contract_tests
)
set_tests_properties(
  abo_instrument_voice_contract_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(
  abo_p1_performance_controls_tests
  abo_p1_performance_controls_tests.cpp
  abo_p1_performance_controls_core.cpp
)
target_include_directories(
  abo_p1_performance_controls_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p1/include
)
enable_keyboard_host_test_assertions(abo_p1_performance_controls_tests)
add_test(
  NAME abo_p1_performance_controls_tests
  COMMAND abo_p1_performance_controls_tests
)
set_tests_properties(
  abo_p1_performance_controls_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(
  abo_score_protocol_tests
  abo_score_protocol_tests.cpp
  abo_score_protocol_core.cpp
)
target_include_directories(
  abo_score_protocol_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_host/include
)
enable_keyboard_host_test_assertions(abo_score_protocol_tests)
add_test(NAME abo_score_protocol_tests COMMAND abo_score_protocol_tests)

add_executable(
  abo_host_wire_tests
  abo_host_wire_tests.cpp
  abo_host_score_protocol_core.cpp
  abo_host_wire_messages_core.cpp
)
target_include_directories(
  abo_host_wire_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_host/include
)
enable_keyboard_host_test_assertions(abo_host_wire_tests)
add_test(NAME abo_host_wire_tests COMMAND abo_host_wire_tests)

add_executable(
  abo_p3_protocol_red_tests
  abo_p3_protocol_red_tests.cpp
  abo_p2_performance_controller_core.cpp
  abo_host_wire_messages_core.cpp
)
target_include_directories(
  abo_p3_protocol_red_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_p2/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../abo_host/include
)
enable_keyboard_host_test_assertions(abo_p3_protocol_red_tests)
add_test(NAME abo_p3_protocol_red_tests COMMAND abo_p3_protocol_red_tests)
set_tests_properties(
  abo_p3_protocol_red_tests
  PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    ENVIRONMENT "PATH=C:/Espressif/tools/mingw64/mingw64/bin"
)
