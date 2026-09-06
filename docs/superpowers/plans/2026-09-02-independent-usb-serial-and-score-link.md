# Independent USB-Serial and Score Link Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a testable independent A Band of One firmware baseline whose only product control channel is ESP32-S3 USB-Serial/JTAG, and make `console/index.html` perform a verified handshake and atomic `abo.score` upload.

**Architecture:** Keep the course motherboard's proven GPIO8, key, encoder, LED, I2S/audio, partition, and boot-flash platform code, but start a new A Band of One product entry path that does not construct or initialize HID/BLE/App transports. A thin line-oriented USB-Serial/JTAG adapter parses JSON into a host-testable transaction core. The browser treats the board as connected only after an application handshake and treats the board as the sole truth for loaded score and runtime state.

**Tech Stack:** ESP-IDF 5.5.5, ESP32-S3 USB Serial/JTAG driver, C++17, ESP-IDF cJSON component at the platform boundary, CMake/CTest host tests, single-file HTML/CSS/JavaScript, Node.js built-in test/assert APIs, Chrome/Edge Web Serial.

**Spec:** `docs/技术方案.md` (§2, §4–§6); UI boundary: `docs/Web控制台功能框架.md`.

## Global Constraints

- [ ] Read `AGENTS.md` items 1–6 before each new implementation session and follow the newest `flow/进展.md` handoff.
- [ ] Do not reinstall ESP-IDF, create a venv, or run `eim install/fix`.
- [ ] Never run `erase_flash`; obtain explicit user confirmation before every flash.
- [ ] Compile only from a pure ASCII copy under `C:\esp-work`; assemble the environment with `. C:\esp-work\tools\idf-env.ps1`.
- [ ] Preserve the exact 16MB partition table and 3MiB factory partition; do not touch `sound_a`/`sound_b` contents.
- [ ] Do not add TinyUSB CDC as a substitute: the final runtime channel is the built-in USB-Serial/JTAG peripheral.
- [ ] Keep audio timing and judging off the browser path; the browser sends requests and renders confirmed board state only.
- [ ] Do not change page layout/style owned by the UI agent except the minimum DOM hooks/status text required by protocol behavior.

---

## Task 1: Extract and test the browser protocol core

**Files:**

- Create: `console/index.test.mjs`
- Modify: `console/index.html`

- [x] **Step 1: Write RED tests for score validation.**

  Add Node assertions that extract the protocol core from `index.html` and reject: wrong/missing `schema`, non-integer/wrong `version`, invalid/non-ASCII/overlong `id`, title over 48 UTF-8 bytes, non-integer/out-of-range BPM, 0 or >128 notes, non-integer/out-of-range `n`, and `b` outside 0.25–8 or not a 0.25 multiple.

  Run:

  ```powershell
  node console/index.test.mjs
  ```

  Expected: FAIL because the current validator accepts at least schema/version/id/BPM and note-shape violations.

- [x] **Step 2: Write RED tests for canonical CRC and chunking.**

  Add a fixed score vector and expected CRC-32/ISO-HDLC value generated from the exact byte stream in `docs/技术方案.md`; assert UTF-8 title encoding, little-endian fields, 8-character lowercase output, 16-note maximum chunks, monotonically increasing `seq`, transport `v:1` on every message, `schema:"abo.score"` plus `score_version:1` on begin, and a shared 8-hex `tx` on begin/chunk/commit.

  Run `node console/index.test.mjs`.

  Expected: FAIL because the current page hashes JSON text and does not attach a transaction ID.

- [x] **Step 3: Implement the smallest pure core and make tests GREEN.**

  Expose a UI-independent `AboConsoleCore` from the embedded script with:

  ```js
  validateScore(value) -> { ok, score?, errors }
  canonicalScoreBytes(score) -> Uint8Array
  crc32Hex(bytes) -> string
  buildScoreUpload(score, tx) -> Array<object>
  isHello(message) -> boolean
  ```

  Keep rendering code separate from these functions. Escape imported text by assigning `textContent` or constructing elements; never inject imported title/id through `innerHTML`.

  Run:

  ```powershell
  node console/index.test.mjs
  node --check console/index.test.mjs
  ```

  Expected: PASS.

---

## Task 2: Make the Web Serial lifecycle deterministic

**Files:**

- Modify: `console/index.test.mjs`
- Modify: `console/index.html`

- [x] **Step 1: Add RED tests with fake SerialPort/reader/writer objects.**

  Cover: VID filter `0x303A`; open at 115200; opened-but-not-handshaken remains “connecting”; `ping` write; `hello(product=abo, protocol=1)` plus first `state` enables both connect buttons; split UTF-8 and split newline across chunks; 2-second handshake timeout; user disconnect; read error; USB disconnect event; `cancel()`, `releaseLock()` and `close()` called exactly once; no use of nonexistent `writer.release()`.

  Run `node console/index.test.mjs`.

  Expected: FAIL against the current connection code.

- [x] **Step 2: Implement one connection owner.**

  Use one persistent `TextDecoder` in streaming mode, one line buffer, one reader, one writer and one abort/cleanup path. All `.btn-connect` controls render from the same connection state. Ignore diagnostic lines that are not valid JSON, but surface protocol-shaped `error` messages globally.

  Run:

  ```powershell
  node console/index.test.mjs
  node --check console/index.test.mjs
  ```

  Expected: PASS; no stale pending promise after disconnect.

- [x] **Step 3: Add upload ACK/timeout/rollback tests and implementation.**

  Send one command at a time; wait for an `ack` matching both `tx` and command name before continuing. Reject matching `error`, disconnect, or 2-second timeout. Update `state.score` and “loaded” UI only after `ack(score_commit)`; preserve the previously confirmed score on every failure.

  Run `node console/index.test.mjs`.

  Expected: PASS for success, bad CRC error, mismatched ACK ignored, timeout, and disconnect rollback cases.

---

## Task 3: Add a host-testable firmware score transaction core

**Files:**

- Create: `firmware/course-motherboard/abo_host/include/abo_host/score_protocol.h`
- Create: `firmware/course-motherboard/abo_host/score_protocol.cpp`
- Create: `firmware/course-motherboard/host_test/abo_score_protocol_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: Write RED C++ tests.**

  Cover the same validator and fixed canonical CRC vector as Task 1, begin/chunk/commit happy path, wrong `tx`, duplicate/out-of-order `seq`, wrong chunk count, note-count mismatch, bad CRC, new begin replacing only staging, and all failures preserving the last committed score.

  Configure and run in a host build directory:

  ```powershell
  cmake -S firmware/course-motherboard/host_test -B firmware/course-motherboard/host_test/build-abo
  cmake --build firmware/course-motherboard/host_test/build-abo --config Debug --target abo_score_protocol_tests
  ctest --test-dir firmware/course-motherboard/host_test/build-abo -C Debug -R abo_score_protocol_tests --output-on-failure
  ```

  Expected: configure/build FAIL because the core does not exist.

- [x] **Step 2: Implement bounded pure C++ state.**

  Define fixed limits and value types (`ScoreMetadata`, `Note`, `UploadSession`, `CommittedScore`, `ProtocolError`) with no ESP-IDF headers and no dynamic allocation in the commit path. Implement the canonical byte CRC exactly once in this core.

  Re-run the three commands above.

  Expected: PASS.

---

## Task 4: Build the independent product entry and USB-Serial/JTAG adapter

**Files:**

- Create: `firmware/course-motherboard/abo_host/include/abo_host/host_link.h`
- Create: `firmware/course-motherboard/abo_host/host_link.cpp`
- Create: `firmware/course-motherboard/main/abo_app_main.cpp`
- Modify: `firmware/course-motherboard/main/CMakeLists.txt`
- Modify: `firmware/course-motherboard/CMakeLists.txt`
- Modify: `firmware/course-motherboard/sdkconfig.defaults`
- Create: `firmware/course-motherboard/host_test/abo_independent_release_contract_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: Add RED source-contract checks.**

  Assert that the A Band of One release gate selects `abo_app_main.cpp`, enables USB-Serial/JTAG, does not compile or initialize `platform/usb_hid.cpp`, `platform/ble_hid.cpp`, `esp_hid`, TinyUSB or BT in that release, and still compiles the board power/key/encoder/LED/speaker platform files. Assert factory/partition constraints remain unchanged.

  Run:

  ```powershell
  cmake --build firmware/course-motherboard/host_test/build-abo --config Debug --target firmware_source_contract_tests
  ctest --test-dir firmware/course-motherboard/host_test/build-abo -C Debug -R firmware_source_contract_tests --output-on-failure
  ```

  Expected: FAIL because the current entry unconditionally starts BLE and HID.

- [x] **Step 2: Add an explicit product build gate.**

  Add an `ABO_PRODUCT` CMake option defaulting ON for this project checkout. Under this gate, compile `abo_app_main.cpp`, `abo_host`, and only the required platform sources/dependencies. Keep the old course entry source available for recovery builds but never compile two `app_main` definitions together. Remove BT/esp_hid/TinyUSB requirements from the A Band of One component graph; preserve exact partition and PSRAM checks, deleting only release checks whose sole purpose was the removed BLE product.

- [x] **Step 3: Implement bounded line transport.**

  Install/use the ESP-IDF USB Serial/JTAG driver, accumulate at most 767 bytes plus terminator, reject oversized lines, parse complete JSON with cJSON only at the adapter boundary, and dispatch typed commands to `score_protocol`. Implement:

  ```text
  ping{v:1} -> hello{v:1,product:"abo",protocol:1,firmware:<version>} then state{v:1,...}
  score_begin/chunk/commit -> matching ack{cmd,tx,...} or error{cmd,tx,reason}
  ```

  Do not block the audio/input loop on USB writes. The browser parser may ignore non-JSON diagnostics; protocol output itself must always be one complete JSON object per line.

- [x] **Step 4: Initialize only proven hardware services.**

  In `abo_app_main.cpp`, retain the existing peripheral power, keys, encoder, LED and speaker startup ordering. Keep the verified piano C4 boot sound. Route input events to an A Band of One stub state (no HID report generation) until P1/P2 attach. Confirm GPIO0, GPIO11, GPIO19/20 and I2S/LED pins are unchanged.

  Re-run all host tests:

  ```powershell
  cmake --build firmware/course-motherboard/host_test/build-abo --config Debug
  ctest --test-dir firmware/course-motherboard/host_test/build-abo -C Debug --output-on-failure
  ```

  Result: PASS. The source-contract and score-protocol targets both pass in
  the ASCII host test build; the ESP-IDF product image also links successfully.

---

## Task 5: Build from the required ASCII workspace without flashing

**Files:**

- Read/verify: `firmware/course-motherboard/partitions.csv`
- Generated only in ASCII copy: `C:\esp-work\abo-independent-fw\build\`

- [x] **Step 1: Copy to and verify the exact ASCII target.**

  Use a non-destructive copy to `C:\esp-work\abo-independent-fw`; never delete or overwrite an unverified broad path. Compare source counts or a scoped checksum manifest before building.

- [x] **Step 2: Assemble the existing environment and build.**

  ```powershell
  . C:\esp-work\tools\idf-env.ps1
  idf.py --version
  idf.py -B build -DABO_PRODUCT=ON build
  ```

  Working directory: `C:\esp-work\abo-independent-fw`.

  Expected: `ESP-IDF v5.5.5`; build succeeds; app is below 2.4MiB and fits the 3MiB factory partition.

- [x] **Step 3: Inspect artifacts without flashing.**

  Record binary size, partition table, ELF component/dependency evidence showing no HID/BLE/TinyUSB product path, and the exact `.bin` path. Stop and request user approval before any board mutation.

  Result: PASS. `C:\esp-work\abo-independent-fw\build\easy_input_keyboard.bin`
  is 360,592 bytes (`0x58090`) and fits the 3 MiB factory partition; the ELF
  map/symbol scan identifies `abo_app_main`, `HostLink`, and `ScoreProtocol`
  without defined HID/BLE/TinyUSB/NimBLE runtime symbols. No flash command was
  run.

---

## Task 6: Perform the user-approved hardware acceptance

**Files:**

- Update after evidence: `flow/进展.md`
- Update if needed: `flow/踩坑记录.md`, `flow/decisions.md`

- [ ] **Step 1: Ask for explicit flash approval and identify the board.**

  User short-presses BOOT only when instructed. Enumerate present devices/ports and choose the actual Espressif download port. Do not use stale COM registry records.

- [ ] **Step 2: Flash without erase.**

  Run only after approval:

  ```powershell
  . C:\esp-work\tools\idf-env.ps1
  idf.py -p COMx flash
  ```

  Never add `erase_flash`.

- [ ] **Step 3: Cold-boot acceptance.**

  Power off/on normally without BOOT. Verify five-light startup feedback, piano C4 boot sound, key and encoder events, a normal runtime serial device, no EasyInput HID enumeration, no EasyInput BLE advertisement, and no EasyInput App connection. Record actual runtime VID/PID/COM as evidence, not as an assumed requirement.

- [ ] **Step 4: Web Serial protocol acceptance.**

  Serve/open `console/index.html`, connect, and verify `ping → hello/state`; upload a valid built-in score; inject a bad CRC and confirm the previous score remains; unplug USB and confirm both connect controls and pending state reset; reconnect and repeat once.

- [ ] **Step 5: Hand off to P1'.**

  Prepend a six-field `project-flow-cy` handoff. Mark T03-U/T05-B complete only when all hardware evidence exists. The next implementation task is T03 P1' VoiceEngine; known violin/clarinet timbre issues remain recorded and are not reopened during transport work.

---

## Final Self-Review Placeholders

- [ ] **Spec coverage:** Every requirement in `docs/技术方案.md` §4.1/§4.2 maps to at least one browser test and one firmware test or explicit HIL step.
- [ ] **Type consistency:** transport `v`, `schema`, `score_version`, `n`, `ticks`, BPM, count, seq, tx and CRC limits/endianness match in JavaScript, C++ and examples.
- [ ] **Boundary review:** No HID/BLE/App startup or dependency remains in the A Band of One product graph; no board pin or partition drift occurred.
- [ ] **Failure review:** Timeout, malformed JSON, oversized line, wrong tx/seq/count/CRC, disconnect and reconnect all preserve the last committed score and clear browser pending state.
- [ ] **Cross-model review:** A model other than the implementation model reviews the diff and test evidence before any claim of completion.
- [ ] **Final verification:** Fresh host tests, Node tests and ASCII-path ESP-IDF build are run immediately before reporting completion; HIL claims include user-observed evidence.
