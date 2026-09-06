# P3' Web 控制台与最终真机验收 Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> **2026-09-05 Task 0 修订**：本计划与 `docs/superpowers/plans/2026-09-05-p3-web-device-feedback-review.md` 已同步。若历史文字与“网页不远程开始/暂停/继续、暂停即 Standby、`.abo.score.json` 完整支持、板端进度为真相”冲突，以 2026-09-05 合同为准。

**Goal:** 将已完成的控制台 UI 接到 P2 板端真相，完成 Standby 模式切换、音量、实体旋钮开始/暂停/继续、`.abo.score.json` 乐谱装载、红蓝引导、拍长游标、成绩和断线恢复，并通过从选曲到首次发声不超过 15 秒的最终真机流程。

**Architecture:** 沿用单文件 `console/index.html` 与现有 Web Serial owner。所有网页改变设备状态的操作都发送带 8 位 `tx` 的请求，等待匹配 ACK 和随后 state；板端用固定容量 TX 队列发送不可丢的 ACK/error/judge/result，并将频繁 state 合并为最新快照。网页不乐观推进谱面、不自行判定、不控制采样级音频时序。

**Tech Stack:** 单文件 HTML/CSS/JavaScript、Chrome/Edge Web Serial、Node.js 内建测试、ESP-IDF USB-Serial/JTAG、C++17、cJSON 边界、CMake/CTest、P2 PerformanceController。

**Spec:** `docs/Web控制台功能框架.md`、`docs/技术方案.md` §4/§6 P3'、`flow/plan.md` T05-C/T06、PRD §04/§09。

## Global Constraints

- [ ] P2 必须已通过宿主、ASCII 构建和真机验收；若顶部交接没有 P2 HIL 证据则停止。
- [ ] 页面视觉由用户的 UI agent 所有；只修改协议行为、状态绑定、必要提示和已有控件 hook，不重排页面、不重做样式。
- [ ] 先获用户对任务组的确认；每次烧录单独确认。不得重装环境、不得从 E: 构建、不得执行 Flash 全盘擦除。
- [ ] 正常产品只用 USB-Serial/JTAG；不恢复 HID/BLE/EasyInput App。
- [ ] 任何断线、超时、坏 CRC 或板端拒绝都保留板端最后有效 score/state；页面清除 pending，不显示虚假成功。
- [ ] 实现模型为 Luna 时，最终审稿必须换非 Luna 模型。

## Frozen Control Messages

所有可变更设备状态的请求均含 `t/v/tx`；`v` 固定为 1，`tx` 为 8 位小写十六进制，同一时刻网页只允许一个 mutating request 在途。

```json
{"t":"set_mode","v":1,"tx":"12ab34cd","mode":"score"}
{"t":"set_volume","v":1,"tx":"12ab34ce","value":70}
{"t":"transport","v":1,"tx":"12ab34cf","action":"reset"}
```

- `set_mode.mode` 仅为 `score|free`，只允许 Standby；暂停与 Standby 为同一板端阶段，因此暂停后可切换；Playing 时返回 `error(reason="invalid_phase")`。切换模式清零旧模式本次进度。
- `set_volume.value` 为整数 0--100，RAM 生效，不写 NVS；非法值拒绝而非静默钳制。
- `transport.action` 在本期只允许 `reset`，且仅 Finished 接受；其他阶段返回 `invalid_phase`。网页不远程 start/pause/resume，实体旋钮是开始/暂停/继续的唯一入口。
- 成功顺序固定为 matching `ack{cmd,tx}`，随后 `state`。网页只在 state 中看到目标值后结束“同步中”。
- 本期不增加网页远程 `key_down/key_up` 或 `encoder_*`；屏幕键盘在连接真机时是状态显示器，不成为第二套演奏输入，避免浏览器失焦导致卡音。

板端状态与事件字段冻结为：

```json
{"t":"state","v":1,"mode":"score","phase":"playing","subphase":"waiting","instrument":0,"octave":4,"volume":70,"bpm":90,"score_loaded":true,"score_id":"twinkle","cursor":0,"errors":0,"elapsed_ms":0,"instrument_loading":false,"knob":0,"leds":[false,false,false,false,false],"keys":["waiting","idle","idle","idle","idle","idle","idle","idle"]}
{"t":"judge","v":1,"ok":false,"pressed":1,"expected":0,"cursor":0,"errors":1}
{"t":"result","v":1,"score_id":"twinkle","errors":1,"elapsed_ms":12345}
```

状态可选增量字段为 `score_crc32`、`note_ticks`、`note_total_ticks`；当前活动音必须满足 `0 <= note_ticks <= note_total_ticks`，Waiting/Standby/Finished 或无活动音时两者为 0。实体 S1--S9 另通过不可合并的 `input` 事件回传 `seq:uint32`、`control:s1..s9`、`phase:pressed|released`，用于 S9 瞬态反馈和 Finished 确认；S1--S8 持续颜色仍以 `state.keys` 为准。

## Task 0：冻结 P3 合同（已完成，2026-09-05）

已将本计划与 `flow/plan.md`、`docs/技术方案.md`、`docs/Web控制台功能框架.md` 对齐：

- 网页只在 Standby 请求 `set_mode`，在 Standby/Playing 请求 `set_volume`，只在 Finished 请求 `transport(action="reset")`；网页不远程 start/pause/resume，实体旋钮负责开始/暂停/继续。
- `state` 的 `score_crc32`、`note_ticks`、`note_total_ticks` 和不可合并的 `input` 事件已进入合同；Finished 的实体输入先被板端消费，再回 Standby。
- 本期只完整支持 UTF-8 `.abo.score.json`；内置《小星星》为 42 音；谱面按拍数横向连续滚动；导入区提供静态外部 AI 提示词，不内置 AI/API/上传。

---

## Task 1: 将控制协议写入合同并实现纯解析/格式化核心

**Files:**

- Modify: `docs/技术方案.md`
- Modify: `docs/Web控制台功能框架.md`
- Modify: `flow/decisions.md`
- Create: `firmware/course-motherboard/abo_host/include/abo_host/control_protocol.h`
- Create: `firmware/course-motherboard/abo_host/control_protocol.cpp`
- Create: `firmware/course-motherboard/host_test/abo_control_protocol_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [ ] **Step 1: 写命令校验 RED。**

  覆盖正确命令、缺失/错误 v、非法 tx、非法 mode/value/action、错误 phase、重复 tx、超出范围音量。控制核心只返回 typed request/ProtocolError，不包含 cJSON 或 ESP-IDF；`transport` 只接受 Finished 下的 `reset`。

- [ ] **Step 2: 更新两份协议合同与决策。**

  将上面的请求、ACK/state 顺序、断线语义和“不支持网页远程演奏”写入文档；不改已有 score_begin/chunk/commit 和 CRC 规范。

- [ ] **Step 3: 实现最小纯 C++ 控制核心。**

  成功请求转换为 P2 action；失败不改变 P2 状态。`set_volume` 使用 0--100→Q15 的确定性映射，最终 PCM 乘法在 P1 mixer 完成并饱和。

  Expected: `abo_control_protocol_tests` 与全部 P1/P2 测试 PASS。

---

## Task 2: 让 HostLink 的关键回包不会因短写丢失

**Files:**

- Create: `firmware/course-motherboard/abo_host/include/abo_host/tx_queue.h`
- Create: `firmware/course-motherboard/abo_host/tx_queue.cpp`
- Create: `firmware/course-motherboard/host_test/abo_host_tx_queue_tests.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/host_link.h`
- Modify: `firmware/course-motherboard/abo_host/host_link.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [ ] **Step 1: 写短写/背压 RED。**

  覆盖一行分多次写、0-byte 暂时背压、保留未写 offset、FIFO、队满、断线清空。ACK/error/judge/result 必须可靠排队；连续 state 只保留最新一份，避免按键事件淹没 TX。

- [ ] **Step 2: 实现固定容量 TX 队列。**

  预留 8 个关键事件槽，每行最大 768B；另设一个 latest-state 槽。`HostLink::poll()` 每轮有限度 flush，不阻塞 input/audio；短写保留剩余字节，下轮续写。

- [ ] **Step 3: 接入控制命令与 P2 快照。**

  cJSON 只验证线格式并调用 `ControlProtocol`；成功动作交给 P2，ACK 后排 state；judge/result 由 P2 action 产生。正式协议行不得混入 ESP_LOG 文本。

  Expected: tx queue、wire、score、control 与 P2 测试全部 PASS。

---

## Task 3: 扩展浏览器纯协议核心

**Files:**

- Modify: `console/index.test.mjs`
- Modify: `console/index.html`

- [ ] **Step 1: 写网页控制请求 RED。**

  在 fake Serial 上覆盖 `set_mode/set_volume/transport(reset)` 的 tx、matching ACK、随后 state 确认、error、2 秒超时、ACK tx 不匹配、ACK 后 state 不一致、断线取消。现有 `.abo.score.json` 上传和 UTF-8 chunk 测试必须保留。

- [ ] **Step 2: 扩展唯一 SerialManager。**

  新增 `sendControl(command)`，复用现有 pending owner，不创建第二 reader/writer。只有 state 确认后 resolve；连接断开时 reject pending、清空 line buffer 和设备快照。

- [ ] **Step 3: 严格校验板端消息。**

  对 state/judge/result 做类型与范围检查；未知字段忽略，已知字段类型错误则显示协议错误且不污染旧 state。保留忽略非 JSON 诊断行的行为。

  Run:

  ```powershell
  node console\index.test.mjs
  node --check console\index.test.mjs
  ```

  Expected: PASS。

---

## Task 4: 把现有 UI 控件绑定到板端真相

**Files:**

- Modify: `console/index.test.mjs`
- Modify: `console/index.html`

- [ ] **Step 1: 模式/工作区绑定。**

  已连接且 Standby 时，点击“跟谱练习/自由演奏”发送 `set_mode`；等待期间显示同步中；state 确认后切实际模式。演奏中点击只显示“模式已锁定”。“乐谱库与导入”只是工作区，不发送 mode。

- [ ] **Step 2: 音量双控件统一。**

  两个滑块共享一个 state。拖动只做视觉预览，80ms 防抖后发送最后值；板端 state 确认后两处同步。断线时恢复未连接态，不把本地 70 当成设备实际值。

- [ ] **Step 3: 实体旋钮反馈与曲终复位绑定。**

  网页旋钮只显示状态，不发送开始/暂停/继续；收到真实 S9 `input` 后两个工作区同步显示黑色“按压”脉冲，最短 120ms。完成卡片在实体任意键被板端消费并回 `state.phase=standby` 后关闭；电脑键或鼠标只在 Finished 发送 `transport(reset)`，等待 ACK→Standby。网页不得自行 start timer、推进 cursor 或播放音频。

- [ ] **Step 4: 红蓝键、谱面与成绩绑定。**

  `state.keys=waiting` 显示红色目标；`judge(ok)`/holding 显示蓝色；错音短显错误色但 cursor 不动；`state.cursor` 推进谱面；`result` 显示错误数与 `elapsed_ms`。自由模式只反映实体按住状态。

- [ ] **Step 5: 保留 UI agent 视觉成果。**

  Node 测试断言原三个工作区、关键 DOM id/class 和样式区仍在；不得用协议实现替换整页 HTML。导入标题继续只用 `textContent`。

  导入控件下方新增稳定 `id="ai-score-help"` 的静态说明卡，提示用户将清晰的单声部五线谱交给外部识图 AI，要求输出 `abo.score v1` JSON，保存为 UTF-8 `曲名.abo.score.json`，并在导入前核对音符、拍数与 BPM。该区域不得包含自动转换、API Key 或第三方上传入口。

  Expected: `console/index.test.mjs` PASS；断网 fake serial 下没有未处理 Promise rejection。

---

## Task 5: 全量验证与最终 ASCII 构建

**Files:**

- Build only: `C:\esp-work\abo-final-fw\`
- Update after evidence: `flow/进展.md`
- Update on failures: `flow/踩坑记录.md`

- [ ] **Step 1: 静态与宿主全回归。**

  运行所有 `console/*.test.mjs`、资产 Python tests、完整 CTest。记录测试数量；不得只运行新增目标。

- [ ] **Step 2: 新建最终 ASCII 副本并构建。**

  目标固定 `C:\esp-work\abo-final-fw`；确认路径后非破坏性复制。dot-source `C:\esp-work\tools\idf-env.ps1`，确认 v5.5.5，再 `idf.py -B build -DABO_PRODUCT=ON build`。

- [ ] **Step 3: 发布前预算检查。**

  app <2.4MiB；factory 仍为 3MiB；EIAD 合计 773134B（未启用降级时）；PSRAM 峰值 <2MiB；无 HID/BLE/TinyUSB/EasyInput App 产品符号；记录 bin、elf、map 的路径/大小/SHA-256。

- [ ] **Step 4: 非 Luna 交叉评审。**

  让另一模型只读审查 P1/P2/P3 代码、协议类型一致性、失败回滚和硬件边界。先修完 P0/P1 问题再申请烧录，不以“测试大多通过”放行。

---

## Task 6: T06 最终真机验收

**Files:**

- Update after acceptance: `flow/进展.md`
- Update final decisions/problems if changed: `flow/decisions.md`, `flow/踩坑记录.md`

- [ ] **Step 1: 用户批准后烧录最终镜像。**

  关闭 Web Serial/monitor；用户短按 BOOT；PresentOnly 核实 `VID_303A&PID_1001` 与实际 COM；从 `C:\esp-work\abo-final-fw` 执行标准 flash。正常电源重启，不按 BOOT。

- [ ] **Step 2: 冷启动与独立产品边界。**

  五灯和钢琴 C4 正常；运行时 USB-Serial/JTAG 可见；不出现 EasyInput HID/BLE/App。S1 短长音不回归。

- [ ] **Step 3: 15 秒主流程。**

  在项目根目录启动本地 HTTP 服务：

  ```powershell
  & 'C:\Espressif\tools\python\python.exe' -m http.server 8800 --bind 127.0.0.1
  ```

  Edge/Chrome 打开 `http://127.0.0.1:8800/console/index.html`。从选择已导入/内置曲目开始计时：装载→ACK→Standby 下切换 score 模式→实体旋钮开始→按首个红色目标键→真机发声，目标不超过 15 秒。

- [ ] **Step 4: 跟谱、自由和双向控制。**

  跟谱连续错三次不推进、正确键红→蓝保持拍长、暂停排除时间、曲终 errors/time 与板端一致；自由模式无评分；网页模式、音量与曲终 reset 请求均由板端 state 回显；实体 S8/旋钮变化同步到网页，网页不发送 start/pause/resume。

- [ ] **Step 5: 事务失败与断线恢复。**

  正确曲提交成功；坏 CRC/乱序/超时明确失败且旧曲可再次开始；演奏中拔线不影响板端音频和判定，网页两个连接按钮、pending、scoreLoaded 和设备状态清空；重连后用首份 state 恢复真实状态，无幽灵值。

- [ ] **Step 6: 回退演练与最终交接。**

  不实际擦除任何数据。确认已保存 P1 C4 基线与最终 bin 的 SHA-256，并记录：恢复 A Band of One 旧版本或课程 EasyInput 都需要重新烧录对应镜像，双方切换都不做全盘擦除。完成六字段交接条和 C7/C8 方法论证据指针。

## Final Self-Review

- [ ] JS/C++ 的 v、tx、mode、phase、volume、cursor、elapsed_ms、score_id 类型完全一致。
- [ ] ACK 后 state、超时、断线、坏 CRC、乱序和 TX 短写均有自动测试与对应 HIL。
- [ ] 页面不自行判定、计拍或发真机音频；连接真机时屏幕键盘不会产生远程卡音。
- [ ] 最终 app/PSRAM/延迟/切仓/15 秒指标都有当次证据，不复用旧构建数据。
- [ ] 非 Luna 模型完成发布前审查，用户完成最终听感与真机验收。
