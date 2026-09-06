# P2' 跟谱/自由双模式状态机 Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 P1 的 63 组合声部与连续 I2S 会话之上，实现板端唯一真相的跟谱练习、自由演奏、严格判定、暂停/恢复、成绩、旋钮语义和 D1--D5 节奏光效，并用板端内置测试谱完成真机闭环。

**Architecture:** 新增无 ESP-IDF 依赖的 `abo_p2::PerformanceController`。控制器接收经过消抖的按键/旋钮事件与“已输出 I2S frame 数”，输出有界动作队列；平台层只执行发声、换仓、LED 和串口状态发布。跟谱的谱面游标只在正确音完整播放后推进，等待按对时谱面冻结但总用时继续，主动暂停时两者都冻结。

**Tech Stack:** C++17、固定容量状态/动作队列、P1 VoiceSession、I2S 48kHz frame clock、CMake/CTest、ESP-IDF 5.5.5、WS2812。

**Spec:** `a-band-of-one-prd/a-band-of-one-prd.html` §04/§05/§09、`a-band-of-one-prd/assets/prototype.js`、`docs/技术方案.md` §4.3/§6 P2'、`flow/plan.md` T04。

## Global Constraints

- [ ] P1 必须已有完整宿主、构建和真机验收交接；若 `flow/进展.md` 顶部仍写“单根 C4”或“P1 待验”，立即停止。
- [ ] 先获用户批准再实现；烧录另行批准。不得重装环境、不得从 E: 直接构建、不得执行 Flash 全盘擦除。
- [ ] 浏览器不参与判定、发声、拍长或计时；P2 可通过串口观察，但不依赖最终网页 UI。
- [ ] 控制器无动态分配、无 ESP-IDF 头、无 sleep/delay；所有时间推进只来自 I2S 已输出 frame 数。
- [ ] 小提琴/单簧管听感优化不混入本任务；P2 只消费 P1 已验收的音色接口。

## Frozen State Semantics

| 项目 | P2 行为 |
|---|---|
| 模式 | `score` 跟谱、`free` 自由；仅 Standby 可切换 |
| 主阶段 | `standby`、`playing`、`finished`；暂停直接使用 `standby` |
| 跟谱子阶段 | `waiting` 等待正确键、`holding` 正确音按谱面时长播放 |
| 正确键 | 立即 `judge(ok)`、发当前音；蓝色状态保持 `note.ticks`；结束后游标推进 |
| 错误键 | 错误数 +1、`judge(error)`、游标不动；错误音最多试听 120ms |
| holding 时再按键 | 忽略且不加错，避免一音尚未结束时重复计分 |
| 跟谱 key-up | 不提前截断；拍长由板端谱面决定 |
| 自由模式 | press 发声、matching release 淡出；不产生 judge/result |
| 暂停 | 停当前音并保留当前谱面索引；暂停时间不计入总用时 |
| 恢复 | 回到同一索引的 waiting，用户重新按该音，重新播放完整拍长 |
| 旋钮旋转 | Playing 循环切换 C3/C4/C5；Standby/Finished 不切换模式或音区 |
| 旋钮按压 | Standby→Playing（开始或继续）；Playing→Standby（暂停）；Finished→Standby |
| S8 | 循环钢琴→小提琴→单簧管；holding 中请求延后到该音结束后执行 |
| 时间 | 48kHz 输出 frame 为唯一时基；waiting 计总用时但不推进乐谱，Standby 暂停时两者都不推进 |

模式切换由网页通过 USB-Serial/JTAG `set_mode` 下发；板端仅在 Standby 接受，旋钮按压不再改变模式。Standby 内切换模式会清零旧模式本次进度；未切换模式时再次按旋钮继续。

---

## Task 1: 定义纯状态与动作合同

**Files:**

- Create: `firmware/course-motherboard/abo_p2/include/abo_p2/performance_types.h`
- Create: `firmware/course-motherboard/abo_p2/include/abo_p2/performance_controller.h`
- Create: `firmware/course-motherboard/abo_p2/performance_controller.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p2_performance_controller_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: 写状态迁移 RED。**

  用内置谱 `p2-scale`：BPM 60、七音 `n=0..6`、每音 96 ticks。覆盖四个主阶段、两个模式、waiting/holding、旋钮按压、演奏中模式锁定、finished 回待机。

- [x] **Step 2: 写严格判定 RED。**

  初始目标 S1；连续按错 S2 三次后 cursor 仍 0、errors 为 3；等待 2 秒 cursor 不动但 elapsed 增加；按 S1 后进入 holding；输出 48000 frames 后 cursor 变 1。holding 期间多按键不加错。

- [x] **Step 3: 写暂停与自由模式 RED。**

  holding 半拍时暂停，立即 note-off；暂停期间推进任意 frames，elapsed/cursor 不变；恢复后回到同一 note 的 waiting。自由模式 press/release 控制声音，不产生 judge/result。

- [x] **Step 4: 实现最小控制器。**

  状态持有 `mode/phase/subphase/instrument/octave/volume/cursor/errors/elapsed_frames/note_frames/pending_instrument`。动作固定容量至少 16，包含 `NoteOn/NoteOff/LoadInstrument/LedEvent/Judge/StateChanged/Result`；队满必须报告错误计数，不能静默覆盖关键动作。

  Run:

  ```powershell
  cmake --build build-host --target abo_p2_performance_controller_tests
  ctest --test-dir build-host -R abo_p2_performance_controller_tests --output-on-failure
  ```

  Expected: PASS。

---

## Task 2: 用有理数累加器实现 I2S frame→tick

**Files:**

- Create: `firmware/course-motherboard/abo_p2/include/abo_p2/performance_clock.h`
- Create: `firmware/course-motherboard/abo_p2/performance_clock.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p2_performance_clock_tests.cpp`
- Modify: `firmware/course-motherboard/abo_p2/performance_controller.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: 写无漂移 RED。**

  公式固定为 `frames × bpm × 96 / (48000 × 60)`；保留余数，不用浮点。覆盖 BPM 40/60/90/240、0.25/1/8 拍、不同块长 128/480、连续 10 分钟误差不超过 1 frame。

- [x] **Step 2: 实现 64-bit 分子/余数累加。**

  `elapsed_frames` 在 Playing 的 waiting/holding/free 都增加；`score_ticks` 只在 holding 增加；Standby/Finished 不增加。BPM 只取已提交 score 或内置测试谱，不在演奏中变化。

- [x] **Step 3: 把音符边界变成动作。**

  note ticks 达标后按顺序发 `NoteOff`、cursor++、`StateChanged`；末音再发 `Result` 并进入 Finished。不得用 FreeRTOS delay 或网页 timer 代替。

  Expected: clock 与 controller 测试 PASS。

---

## Task 3: 固定 D1--D5 光效语义

**Files:**

- Create: `firmware/course-motherboard/abo_p2/include/abo_p2/rhythm_led_model.h`
- Create: `firmware/course-motherboard/abo_p2/rhythm_led_model.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p2_rhythm_led_tests.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p2_rhythm_led_core.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`
- Modify: `firmware/course-motherboard/main/platform/led_strip_status.h`
- Modify: `firmware/course-motherboard/main/platform/led_strip_status.cpp`

- [x] **Step 1: 写 LED 模型 RED。**

  Playing 时每个四分音符将一个蓝色节拍点按 D1→D5 循环；正确键产生一次绿色短脉冲但不改变网页红/蓝键语义；错误键五灯红闪 120ms；暂停进入 Standby 后灯灭；Finished 播放一次五灯彩色往返后定格熄灭。

- [x] **Step 2: 实现纯颜色帧模型。**

  模型只输出五个 RGB 值；平台层继续复用 `StatusLedStrip` 与 GPIO8 已验证上电顺序，不创建第二个 RMT/WS2812 驱动。

- [x] **Step 3: 验证冷启动序列优先级。**

  P0 五灯启动序列未结束前，P2 光效不得抢占；启动结束后才由状态机接管。

  Expected: LED 模型测试和既有 boot LED 测试全部 PASS。

---

## Task 4: 接入已提交乐谱和平台动作

**Files:**

- Create: `firmware/course-motherboard/abo_p2/include/abo_p2/score_snapshot.h`
- Create: `firmware/course-motherboard/abo_p2/score_snapshot.cpp`
- Create: `firmware/course-motherboard/main/platform/performance_runtime.h`
- Create: `firmware/course-motherboard/main/platform/performance_runtime.cpp`
- Modify: `firmware/course-motherboard/main/abo_app_main.cpp`
- Modify: `firmware/course-motherboard/main/CMakeLists.txt`
- Modify: `firmware/course-motherboard/main/platform/instrument_voice_session.h`
- Modify: `firmware/course-motherboard/main/platform/instrument_voice_session.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/host_link.h`
- Modify: `firmware/course-motherboard/abo_host/host_link.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/wire_messages.h`
- Modify: `firmware/course-motherboard/abo_host/wire_messages.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p2_platform_contract_tests.cpp`

- [x] **Step 1: 写平台接缝 RED。**

  启动默认装载内置 `p2-scale`；若 `ScoreProtocol` 已有 committed score，则下一次从 Standby 开始时复制该不可变快照。演奏期间新 score_commit 只成为“下一首”，不得替换正在演奏的 score。快照使用固定容量、无动态分配的 `ScoreSnapshot`，不直接持有 `ScoreProtocol` 内部指针。

- [x] **Step 2: 执行控制器动作。**

  `NoteOn/Off` 只调用 P1 session；`LoadInstrument` 走 P1 bank-safe 换仓；LED 走既有 strip；Judge/Result/StateChanged 交给 HostLink。任何平台失败都进入可观察错误态并停止演奏，不继续推进 cursor。

- [x] **Step 3: 将 I2S frame 回报接到控制器。**

  Voice provider 每完成一块，使用原子 64-bit frame counter 累加；main 每轮取增量并调用 controller。不得在音频线程直接格式化 JSON、更新 LED 或操作 score。

- [x] **Step 4: state 输出实际 P2 快照。**

  至少含 `mode/phase/subphase/instrument/octave/volume/bpm/score_id/cursor/errors/elapsed_ms/keys/leds/instrument_loading`。字段类型固定，P3 网页只消费该快照。

  Expected: P2 平台合同、P1 全套、score protocol 与 wire tests 全部 PASS。

---

## Task 5: ASCII 构建与 P2 真机闭环

**Files:**

- Build only: `C:\esp-work\abo-p2-fw\`；旋钮双触发修复候选：`C:\esp-work\abo-p2-fw-encoder-fix\`
- Update after evidence: `flow/进展.md`
- Update on failures: `flow/踩坑记录.md`

- [x] **Step 1: 全量回归与新 ASCII 构建。**

  已复制到新的 `C:\esp-work\abo-p2-fw`，在新建 `host_test/build-task5` 完成全量 CTest `74/74`；使用现有 ESP-IDF `v5.5.5-dirty`、`ABO_PRODUCT=ON` 完成 ESP32-S3 产品构建。应用 BIN 为 `1,642,560B (0x191040)`，3 MiB factory 余量为 `1,503,168B (0x16efc0，47.78%)`，SHA-256 为 `986B65139C91F0C21B81C8677915D0330E23F3EE52D387EAF7FDCC836B36479F`；固定 PSRAM 解码仓为 `1,402,514B`。最低 free PSRAM 属于真机运行指标，构建期不虚报，留待 Step 2/HIL 采集；未覆盖 P1 回退镜像。

- [x] **Step 2: 申请并执行一次标准烧录。**

  用户明确批准、关闭串口、短按 BOOT、PresentOnly 核实 `USB\\VID_303A&PID_1001 / COM5` 后完成标准 flash；bootloader、partition table、app 均 `Hash of data verified`。正常电源重启，不按 BOOT。

- [ ] **Step 3: 跟谱严格判定 HIL。**

  用 `p2-scale`：启动 score；在 S1 为目标时按错 S2 三次，确认 cursor 不动/errors=3；等待 2 秒确认 elapsed 增加；按 S1，确认声音与 holding 持续 1 秒后推进；完成七音得到 result。

  首次 HIL 发现一次旋钮物理点击同时处理了 `Pressed` 与 `Released`，使 score 从 Playing 立即进入旧 Paused；边沿修复与网页 `set_mode` 版本已烧录并完成 63 音/S8 基础 HIL。后续真机暴露旧 Paused 无法回到 Standby、网页因而不能切模式；用户已冻结“暂停 = Standby”，修复候选已完成 RED→GREEN、独立复审、ASCII 全回归与产品构建，Step 3 仍待单独批准烧录后复验。

- [ ] **Step 4: 暂停/恢复 HIL。**

  正确音响到半途按旋钮暂停，确认进入 Standby、声音停止、索引不动、等待 2 秒用时不增加；不切模式再按旋钮恢复，确认回到同一目标 waiting，重按后完整播放该音。

- [ ] **Step 5: 自由模式与控制 HIL。**

  在 Playing 按旋钮进入 Standby，在网页端发送 `set_mode:free` 并等待 `ack/state.mode=free`，确认旧跟谱进度已清零；再按旋钮开始。S1--S7 按住/松开正常，无 judge/result；旋钮只在 Playing 阶段循环三音区；S8 三乐器循环；D1--D5 节拍、错音、暂停、曲终样式符合合同。旋钮按压不得切换自由/跟谱模式。

- [ ] **Step 6: P2 停止门。**

  三次错误、waiting 计时、pause 排除、正确拍长、自由无判定、旋钮/S8/LED 和 result 全部有证据后，更新 `flow/进展.md` 并进入 P3。任一失败只定位对应层，不在此阶段改网页视觉或音源。

## Final Self-Review

- [ ] 对照 `prototype.js` 检查每条状态迁移；对照 PRD §04/§09 检查无遗漏。
- [ ] 搜索禁止时基：P2 核心不得包含 `vTaskDelay`、浏览器 timer 或 wall-clock 判定。
- [ ] 验证 score 提交与当前演奏快照隔离，暂停/恢复和 S8 延后切换无悬空音符。
- [ ] 由非 Luna 模型复审状态机、时间累计和 HIL 证据后再宣称 P2 完成。
