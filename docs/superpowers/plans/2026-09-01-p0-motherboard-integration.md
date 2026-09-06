# P0' Course-Motherboard Integration Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a one-shot P0' cold-boot LED and speaker self-check inside the recovered EasyInput course firmware without changing its HID, BLE, keymap, or App protocol.

**Architecture:** The course firmware remains the sole application. A small pure-C++ P0' state machine is polled only by the existing `app_main` owner; it requests LED and audio effects through the existing course services, then releases control. GPIO8 is never written by P0' code: `PeripheralPowerController::begin_awake()` already establishes the 50 ms safe power sequence and remains its only owner.

**Tech Stack:** ESP-IDF 5.5.5; C++17; existing TinyUSB HID, NimBLE, `StatusLedStrip`, `SpeakerOutput`, `PeripheralPowerController`; MinGW host tests.

**Spec:** `docs/技术方案.md` §2, §6; `a-band-of-one-prd/a-band-of-one-prd.html` §06, §09; `flow/decisions.md` entry “P0' 改为课程母本内自检”。

## Global Constraints

- Compile only from a pure-ASCII copy under `C:\esp-work` after `. C:\esp-work\tools\idf-env.ps1`.
- ESP-IDF version is exactly 5.5.5; never run `erase_flash`.
- The editable source-of-truth is `firmware/course-motherboard/`, a non-destructive source-only copy of the recovered course project; `firmware/`'s legacy standalone P0 files are not a build input.
- Preserve USB/BLE name, TinyUSB descriptor, BLE implementation, Host Action v1 `0x11/kind 0x05`, existing keymap, and all existing partitions.
- P0' does not write GPIO8, configure I2S/RMT, create a competing FreeRTOS task, emit HID reports, or consume S1--S8.
- After any firmware flash, fully exit and reopen EasyInput before App verification.

---

### Task 1: Pure state-machine contract and host RED/GREEN tests

**Files:**
- Create: `firmware/course-motherboard/abo_p0/include/abo_p0/startup_self_check.h`
- Create: `firmware/course-motherboard/abo_p0/startup_self_check.cpp`
- Create: `firmware/course-motherboard/abo_p0/host_test/startup_self_check_test.cpp`
- Create: `firmware/course-motherboard/abo_p0/host_test/CMakeLists.txt`

**Interfaces:**
- Produces `abo_p0::StartupSelfCheck` with `start(bool cold_boot)`, `poll(bool led_ready, bool speaker_ready, bool speaker_busy)`, `take_led_request()`, `take_speaker_request()`, and `phase()`.
- Consumes only booleans supplied by `app_main`; it has no ESP-IDF includes or hardware access.

- [ ] **Step 1: Write the failing host test**

```cpp
TEST(StartupSelfCheck, ColdBootRequestsLedThenOneSpeakerProbeThenCompletes) {
  abo_p0::StartupSelfCheck check;
  check.start(true);
  EXPECT_TRUE(check.take_led_request());
  check.poll(true, true, false);
  EXPECT_TRUE(check.take_speaker_request());
  check.poll(true, true, true);
  check.poll(true, true, false);
  EXPECT_EQ(check.phase(), abo_p0::StartupSelfCheckPhase::Complete);
}
```

- [ ] **Step 2: Run the test and verify RED**

Run: `cmake -S firmware/abo_p0/host_test -B C:\esp-work\abo-p0-host-build && cmake --build C:\esp-work\abo-p0-host-build && ctest --test-dir C:\esp-work\abo-p0-host-build --output-on-failure`

Expected: FAIL because `StartupSelfCheck` does not exist.

- [ ] **Step 3: Implement the smallest dependency-free state machine**

```cpp
enum class StartupSelfCheckPhase { Idle, LedRequested, SpeakerRequested, WaitingSpeaker, Complete, Skipped };
// start(false) enters Skipped. The speaker request is emitted exactly once,
// only after led_ready && speaker_ready && !speaker_busy.
```

- [ ] **Step 4: Run the host test and verify GREEN**

Run: same command as Step 2.

Expected: PASS; add tests for non-cold boot and a busy speaker delaying, rather than duplicating, the request.

### Task 2: Integrate only through existing course-service ownership

**Files:**
- Create: `firmware/course-motherboard/main/abo_p0_startup_bridge.h`
- Create: `firmware/course-motherboard/main/abo_p0_startup_bridge.cpp`
- Modify: `firmware/course-motherboard/main/CMakeLists.txt`
- Modify: `firmware/course-motherboard/main/app_main.cpp`

**Interfaces:**
- Consumes `abo_p0::StartupSelfCheck`, `easy_input::StatusLedStrip`, and `easy_input::SpeakerOutput`.
- Produces `easy_input::AboP0StartupBridge::poll(...)`, invoked once per existing `app_main` loop after the normal speaker service.

- [ ] **Step 1: Write the failing integration-level host test**

```cpp
TEST(AboP0StartupBridge, NeverTouchesKeysOrHidAndReturnsLedControlAfterCheck) {
  FakeLed led;
  FakeSpeaker speaker;
  easy_input::AboP0StartupBridge bridge;
  bridge.start_cold_boot();
  drive_until_complete(bridge, led, speaker);
  EXPECT_EQ(led.raw_show_count, 1);
  EXPECT_EQ(speaker.diagnostic_request_count, 1);
  EXPECT_EQ(bridge.phase(), abo_p0::StartupSelfCheckPhase::Complete);
}
```

- [ ] **Step 2: Run it and verify RED**

Run: extend Task 1 host-test target with the bridge and run `ctest`.

Expected: FAIL because the bridge does not exist.

- [ ] **Step 3: Implement the bridge and narrow `app_main` changes**

```cpp
// Pseudocode in the existing main-loop owner:
service_speaker(&app);
app.abo_p0.poll(app.leds, app.speaker, millis());
// Bridge calls show_raw_color({0, 0, 16}) once and request_diagnostic_tone()
// once. It never accesses GPIO, I2S, RMT, app.inputs, app.usb, or app.ble.
```

Add the two `.cpp` files to `easy_input_main_sources`; begin the bridge only on a cold/restart boot. If the current build has no speaker diagnostic capability, fail closed with the LED-only result and log the missing capability; do not enable a new component until reviewed.

- [ ] **Step 4: Run host tests and a clean ESP-IDF build**

Run: copy the confirmed source to `C:\esp-work\abo-course-p0`, dot-source `C:\esp-work\tools\idf-env.ps1`, then run `idf.py build` there.

Expected: all host tests PASS and a firmware image is produced; inspect its size against the 3 MiB factory partition.

### Task 3: Controlled HIL and recovery verification

**Files:**
- Modify: `flow/进展.md`
- Modify: `flow/踩坑记录.md` only if HIL reveals a new reusable failure.

**Interfaces:**
- Consumes a user-confirmed download-mode COM port and the built image.
- Produces HIL evidence, not a new firmware architecture.

- [ ] **Step 1: Obtain separate permission to flash**

State the exact ASCII build path, target COM port, and that the command is `idf.py -p COMx flash` without `erase_flash`.

- [ ] **Step 2: Flash only after BOOT short-press and port verification**

Run: `idf.py -p COMx flash` from `C:\esp-work\abo-course-p0`.

Expected: write and hash verification only; no `erase_flash`.

- [ ] **Step 3: Perform cold-boot HIL**

Expected: five dim LEDs are visible, exactly one board-speaker click occurs, no abnormal pop; then keyboard HID, BLE pairing, and EasyInput App reconnect normally.

- [ ] **Step 4: Record the outcome**

Put the observed result, image path, and next P1' gate in `flow/进展.md` at the top. If click or LEDs fail, stop before P1' and diagnose through the course-service logs; do not add raw drivers.

## Self-Review

- Spec coverage: GPIO8 safe sequencing is preserved through `PeripheralPowerController`; LED and click satisfy P0'; HID/BLE/App are explicitly protected; flash is separated from build and requires a new approval.
- Placeholder scan: no implementation step relies on unspecified hardware access; the only optional condition (speaker diagnostic feature absent) has a fail-closed result.
- Type consistency: `StartupSelfCheckPhase`, `StartupSelfCheck`, and `AboP0StartupBridge` are the only new public names and are used consistently across tasks.
