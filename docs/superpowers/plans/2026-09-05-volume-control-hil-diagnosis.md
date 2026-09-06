# Web 音量真机故障诊断 Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不猜测、不改音源的前提下，定位“网页音量数值可操作但键盘响度不变”究竟断在网页事务、板端目标状态、音频邮箱还是最终 I2S 输出。

**Architecture:** 以四个边界逐层取证：网页期望值 → ACK/state 确认值 → 音频线程实际应用值 → I2S PCM。先用当前固件做无需改代码的 0/70/100 黑盒测试；只有现有状态不足以区分时才增加独立诊断页，只有板端已确认目标但声音仍不变时才增加低频 applied-volume 遥测。

**Tech Stack:** 单文件 HTML/CSS/JavaScript、Chrome/Edge Web Serial、Node.js 内建测试、C++17、ESP-IDF 5.5.5、CMake/CTest、ESP32-S3 USB-Serial/JTAG。

**Spec:** `flow/plan.md` 的 P3 音量边界、`docs/技术方案.md` §4.3/P3'、`docs/Web控制台功能框架.md` §1/§3/§4。

## Global Constraints

- 当前硬件为 EasyInput V2 / ESP32-S3；运行控制通道固定为 USB-Serial/JTAG，COM5 仅在 PresentOnly 再确认后使用。
- `set_volume.value` 只能为整数 `0..100`，允许阶段为 Standby/Playing；默认 70 必须保持当前 0dB 基线。
- 网页不是声音真相；测试必须分别记录期望值、ACK、`state.volume` 和实体听感。
- 固件构建必须复制到新的纯 ASCII `C:\esp-work` 目录；禁止在 E: 含中文/`#` 路径执行 ESP-IDF 构建。
- 修改代码、生成诊断固件和烧录分别等待用户批准；任何阶段都禁止 `erase_flash`，不修改 23 枚正式音频资产。

## 已取得证据

| 边界 | 当前证据 | 尚缺证据 |
|---|---|---|
| 网页 → HostLink | `console/index.html` 会发送 `set_volume`；其他 Web Serial 功能正常 | 真机是否收到 matching ACK 与目标 `state.volume` |
| HostLink → Runtime | 源码包含解析、回调注册和 state 发布；烧录副本与 E: 真相源 11/11 SHA-256 一致 | 真机 callback 是否以用户值执行 |
| Runtime → Mixer | 目标邮箱、480 帧渐变和 0/70/100 mixer 测试通过 | 音频线程是否消费了本次 mailbox generation |
| Mixer → I2S | 63 音正常发声，证明 voice stream/I2S 基线工作 | 改音量后的最终 PCM 峰值是否变化 |

当前不能直接宣称单一根因。最值得先验证的两个假设是：

1. 网页只收到了 ACK 或目标 state，尚未证明音频线程应用；现有 `state.volume` 表示控制器目标值，不表示 mixer 已完成渐变。
2. 若用户此前主要比较 54 与 70，当前线性幅度映射仅约相差 −2.3dB，可能“变化不明显”；0 必须在 10ms 渐变后静音，可作为客观停止门。

---

### Task 0: 用当前版本完成零改动真机分层测试

**Files:**
- Read: `console/index.html`
- Record after test: `flow/进展.md`
- Record if failed: `flow/踩坑记录.md`

**Interfaces:**
- Consumes: 当前已烧录 COM5 固件、现有 Web 控制台、自由演奏模式。
- Produces: `ui_value`、`reconnect_state_volume`、`held_note_at_0`、`new_note_at_0` 四项结果，用于唯一选择后续边界。

- [ ] **Step 1: 建立可重复基线。**

  关闭其他串口工具，强制刷新 `http://127.0.0.1:8800/console/index.html`，连接 COM5，切到自由演奏并按实体旋钮进入 Playing。确认音量显示 70，短按 S1 有声。

- [ ] **Step 2: 执行持音 70→0→70 测试。**

  长按 S1；保持按住时把滑块从 70 拉到 0，等待 2 秒，再拉回 70，等待 2 秒后松键。记录 0 是否在约 10ms 后静音、70 是否恢复，以及页面数值是否自行跳回。

- [ ] **Step 3: 执行新音与重连测试。**

  将音量设为 0，等待 2 秒后重新短按 S1；随后断开并重新连接，记录首份 `state.volume`。再恢复 70，避免把设备留在静音状态。

- [ ] **Step 4: 按停止门分类。**

  - 页面跳回 70、出现下发失败或重连仍为 70：故障在网页事务/HostLink 确认边界，进入 Task 1。
  - 页面与重连均为 0，但持音和新音仍有声：故障在 Runtime→Mixer 或 Mixer→I2S 边界，跳到 Task 2。
  - 0 能静音，但 30/54/70 主观差异小：功能链成立，问题归类为响度曲线设计；停止本计划，另提“感知响度映射 A/B”方案，不改协议。
  - 持音不变但 0 下的新音静音：邮箱只在音符边界生效，进入 Task 2，并把该现象作为 RED 条件。

- [ ] **Step 5: 记录证据并停止。**

  在 `flow/进展.md` 顶部记录四项结果和选中的唯一分支；未得到明确分类前不修改代码、不重新烧录。

---

### Task 1: 修复并验证 Web Serial 音量事务边界（已确认分支）

**确认依据（2026-09-05）：** 用户反馈右侧音量数字始终为 70、无法显示 0。源码滑块下限是 0；板端主循环每 10ms 轮询、每 100ms 发布一次 `state`，网页输入后却要等待 80ms 才把目标放入 `volumePending`。按现有代码重放“输入 0 → 20ms 后收到旧 state 70 → 80ms 防抖到期”，结果是 `volumeDesired` 从 0 被改回 70，最终发送 70。现有 `console/index.test.mjs` 仍通过，因为它只测试 `SerialManager` 的 ACK/state，不执行 DOM 音量状态机。

**本任务的 RED 合同：** 当用户目标为 0 且存在防抖/在途请求时，收到旧 `state.volume=70` 不得改写目标滑块；防抖到期必须发送 0。若用户在请求在途时继续改为 30，30 必须成为下一次发送值，不能滞留。`setVolume()` 只有在 matching ACK 且后续 `state.volume` 等于请求值后才算确认；明确断开或请求失败时回退到最近已确认值。

**Files:**
- Modify: `console/index.html`
- Modify: `console/index.test.mjs`
- Create only if HIL remains ambiguous: `console/diagnose-volume-hil.html`

**Interfaces:**
- Consumes: `ping → hello/state`、`set_volume{t,v,tx,value}`、matching `ack`、`state.volume`。
- Produces: 每次请求的 `tx`、发送值、ACK 时间、state 时间、确认值和原始 RX 行；不发送模式、乐谱或演奏命令。

- [x] **Step 1: 写失败的 UI 时序测试。**

```js
volume.input(0);
clock.advance(20);
volume.onBoardState(70);
clock.advance(60);
assert.equal(volume.displayed(), 0);
assert.equal(serial.sent.at(-1).value, 0);

volume.input(30); // first request is still in flight
volume.onAckAndState(0);
clock.advance(80);
assert.equal(serial.sent.at(-1).value, 30);
```

- [x] **Step 2: 运行 RED。**

  Run: `node console/index.test.mjs`

  Expected: FAIL，至少命中“旧 state 覆盖目标 0”或“在途最新值滞留”的新断言。

- [x] **Step 3: 实现单一音量协调器。**

```js
// 状态至少区分 desired / confirmed / queued / inFlight。
// input 立即写 queued，80ms 只负责延迟发送；旧 state 不得清除 queued。
// inFlight 完成后若 queued 仍有值，继续发送最新值。
```

  将“用户目标、板端已确认值、等待发送值、在途请求值”分离；输入事件立即登记最新目标，80ms 仅用于合并连续拖动。旧 `state.volume` 不得覆盖本地目标；匹配确认后同步两个工作区。若完成修复后的 HIL 仍出现歧义，再创建只读原始 RX 的独立诊断页，不预先增加页面。

- [x] **Step 4: 运行 GREEN 与语法检查。**

  Run: `node console/index.test.mjs`

  Expected: PASS，且内嵌脚本可由 `new Function()` 解析。

- [x] **Step 5: 人工 HIL 并停止。**

  强制刷新 `http://127.0.0.1:8800/console/index.html`，逐次测试 70→0→30→70→100；确认两个工作区数值同步、0 下新音静音、在途连续拖动不会回 70、断线重连恢复板端已确认值。此 Task 不需要固件构建或烧录。

**Task 1 当前结果（2026-09-05）：** 网页状态竞争修复已完成；`console/index.test.mjs` 与全部 16 个 console 测试通过，两个嵌入脚本均可由 `new Function()` 解析。用户已反馈 COM5 人工 HIL 通过：音量可以调节，并能反馈到键盘；本任务关闭，不需要重新烧录。

---

### Task 2: 增加音频消费者已应用值遥测（仅当 state=0 但仍有声）

**Files:**
- Modify: `firmware/course-motherboard/main/platform/instrument_voice_session.h`
- Modify: `firmware/course-motherboard/main/platform/instrument_voice_session.cpp`
- Modify: `firmware/course-motherboard/main/platform/performance_runtime.h`
- Modify: `firmware/course-motherboard/main/platform/performance_runtime.cpp`
- Modify: `firmware/course-motherboard/host_test/abo_p3_volume_red_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

**Interfaces:**
- Consumes: mailbox target generation、`MonoVoiceMixer::volume()`、`MonoVoiceMixer::target_volume()` 和 `volume_ramp_active()`。
- Produces: main-task 可读取的 `InstrumentVoiceSession::applied_volume()`；仅当 target/applied 变化时输出一条 `P3_DIAG volume_target=N volume_applied=M`，禁止在音频线程打印日志。

- [ ] **Step 1: 写 applied-volume RED。**

```cpp
require_contains(session_header, "applied_volume()",
                 "session exposes the audio-consumer-applied volume", &ok);
require_contains(session_source, "applied_volume_.store",
                 "render consumer publishes the completed mixer volume", &ok);
require_contains(runtime_source, "P3_DIAG volume_target",
                 "main task logs target/applied transitions", &ok);
```

- [ ] **Step 2: 运行 RED。**

  Run: build and execute `abo_p3_volume_red_tests` with `C:\Espressif\tools\mingw64\mingw64\bin` in PATH.

  Expected: FAIL on the three new applied-volume contracts.

- [ ] **Step 3: 实现非实时线程遥测。**

  `render_frame()` 在 `mixer_.render()` 后仅用 atomic store 发布已完成的 mixer volume；`PerformanceRuntime::poll()` 在 main task 比较上次 target/applied 组合，只在变化时打印一次 WARN 级 `P3_DIAG`。不得从 audio provider 调用 `ESP_LOG*`，不得改变 PCM、音源、Q15 映射或现有 state schema。

- [ ] **Step 4: 运行 GREEN 和聚焦回归。**

  Run: `abo_p3_volume_red_tests`、`abo_mono_voice_mixer_tests`、`abo_mode_control_contract_tests`。

  Expected: 3/3 PASS；0 静音、70 位等价、100 饱和和 480 帧渐变断言保持通过。

- [ ] **Step 5: 另行申请构建与烧录。**

  将真相源复制到新的 ASCII 目录，完成 ESP-IDF 5.5.5 构建并记录 bin 大小/SHA-256/factory 余量；向用户单独申请 COM5 烧录。烧录后由诊断页记录 target/applied，定位为“邮箱未消费”或“已消费但最终 PCM/I2S 未变化”。

---

## 修复决策门

本计划只负责把根因定位到唯一边界，不预先修改声音算法：

- ACK/state 失败：另立只修改 `console/index.html` / `abo_host` 事务的最小修复计划。
- target 与 applied 不同：另立只修改 mailbox 消费边界的最小修复计划。
- target 与 applied 相同但 0 仍有声：另立最终 PCM/I2S 峰值探针计划，检查 gain 是否在写 I2S 前被覆盖。
- 0 正常静音但中段差异不足：另立感知响度曲线 A/B 计划；保持 70 为原基线，先网页/宿主试听，再决定是否烧录。

一次只选择一个分支，先 RED 后 GREEN；任何修复完成后重新执行 0→70、持音、新音、重连和三乐器回归。

## Final Self-Review

- [x] 覆盖网页、协议、状态、邮箱、mixer 和 I2S 六个相关边界。
- [x] 没有把“代码存在”当作真机功能通过。
- [x] Task 0 不改代码；Task 1 不烧录；Task 2 的构建与烧录有独立批准门。
- [x] 未修改音频资产、乐器映射、协议字段或默认 70 基线。
