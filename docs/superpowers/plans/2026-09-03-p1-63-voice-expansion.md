# P1' 63 组合声音引擎扩展 Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在已经真机通过的单根钢琴 C4 持续声部基础上，接入 23 枚 EIAD 根音，交付 S1--S7 × 三音区 × 三乐器共 63 个可演奏组合、S8 乐器循环、旋钮音区切换，并保持 3MiB factory、2MiB PSRAM 和按键到声音小于 50ms 的门槛。

**Architecture:** 23 枚 EIAD 压缩资产全部嵌入 factory；运行时只分配一个可容纳最大乐器的固定 PSRAM PCM 仓。音频工作器通过有界单生产者/单消费者命令队列接收 note-on/off，在整个演奏会话持续输出 I2S 帧；重触发由双声部短交叉淡化完成。切换乐器时先让渲染器在帧边界进入静音安全态，再在同一 PCM 仓原地解码新乐器，因此不同时常驻钢琴与小提琴两套 PCM。

**Tech Stack:** ESP-IDF 5.5.5、ESP32-S3、C++17、FreeRTOS、48kHz PCM16、EIAD/IMA-ADPCM、PSRAM、I2S DMA、CMake/CTest、Python 资产契约测试。

**Spec:** `docs/技术方案.md` §3/§6 P1'、`flow/plan.md` T03、`a-band-of-one-prd/a-band-of-one-prd.html` §04/§06/§09。

## Global Constraints

- [ ] 每次开始前按 `AGENTS.md` 1--6 顺序阅读；以 `flow/进展.md` 顶部为当前事实。
- [ ] 本计划是规划，不自动授权实现。Luna 必须先获得用户对当前任务组的明确确认；每次烧录还要单独确认。
- [ ] 不重装 ESP-IDF、不新建 venv、不运行 `eim install/fix`。
- [ ] E: 是源码真相源；任何 ESP-IDF 构建先复制到新的纯 ASCII 目录。不得覆盖当前已验收回退镜像。
- [ ] 禁止执行 Flash 全盘擦除；不改 `sound_a`/`sound_b`，不启动 HID、BLE 或 EasyInput App。
- [ ] 不重新生成、覆盖或重新下载 23 枚正式音频；小提琴/单簧管听感优化已由用户明确延后，只有无声、损坏、错误音高或无法循环才阻断 P1。
- [ ] 每项功能先写会失败的宿主测试，观察 RED，再做最小实现并跑 GREEN；没有真机证据不得声称 P1 完成。
- [ ] 当前项目根目录没有 Git 元数据，不执行虚构的提交步骤；每个任务完成后用测试日志、SHA-256 和 `project-flow-cy` 交接条留证。

## Verified Baseline

- 当前真机镜像：`C:\esp-work\abo-independent-fw\build\easy_input_keyboard.bin`，382672B，SHA-256 `DF1022F009DDDB30C2E6CEC75723E9799971E4BC6CDACD3A5720A5073D932447`。
- 用户已验收：五灯、启动钢琴 C4、S1 短音、S1 持续至少 4 秒无间隔、松键淡出。
- 当前代码只嵌入 `piano_c4.eiad`；`PianoVoiceSession` 只接受 S1/C4；`HostLink::send_state()` 的模式、乐器、音区仍是硬编码值。
- 正式资产：钢琴 7 根（359534B EIAD / 1402514B PCM）、小提琴 7 根（258440B / 1008000B）、单簧管 9 根（155160B / 604800B）；合计 23 根、773134B EIAD、3015314B PCM。
- 钢琴与小提琴双缓存为 2410514B，已超过 2MiB 门槛；本计划因此采用单一固定 PCM 仓，不实现“两套完整乐器同时预取”。

### 根音删减策略

- 默认保留 23 枚根音；不得删除 S1--S7 对应的任何可演奏唱名，否则产品会从 21 音降级。
- 钢琴删任意 1 枚后最坏变调扩大到 5--6 半音；小提琴删根音会削弱此前专门补齐的中音区，不作为首选优化。
- 只有在最终 app 超过 2.4MiB 或单簧管切仓超过 500ms 时，才启用一次降级评审：候选删除单簧管 D4、D5，可省 34480B EIAD 和 134400B 解码量，21 个目标的最坏变调仍为 3 半音。
- 该降级不会降低固定 PCM 仓峰值，因为峰值由钢琴 1402514B 决定；实施前必须先做浏览器 A/B、更新 manifest/技术方案/决策，并重新完成单簧管 21 音真机验收。

---

## Task 0: 关闭 T03-U/T05-B 遗留真机证据

**Files:**

- Create: `firmware/course-motherboard/abo_host/include/abo_host/wire_messages.h`
- Create: `firmware/course-motherboard/abo_host/wire_messages.cpp`
- Create: `firmware/course-motherboard/host_test/abo_host_wire_tests.cpp`
- Create: `firmware/course-motherboard/tools/verify_score_link.py`
- Modify: `firmware/course-motherboard/abo_host/host_link.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/host_link.h`
- Modify: `firmware/course-motherboard/main/CMakeLists.txt`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`
- Modify after HIL only: `flow/tasks/T03-U-独立固件串口链路.md`

- [x] **Step 1: 为可观察的乐谱提交结果写 RED 测试。**

  测试纯 C++ 消息格式器：`ack(score_commit)` 必须含 `score_id`、`score_version`；`state` 必须含 `score_loaded` 与当前 `score_id`。坏 CRC 后再次生成 `state`，仍显示上一首已提交曲目。

  在 ASCII 副本中运行目标测试：

  ```powershell
  cmake --build build-host --target abo_host_wire_tests
  ctest --test-dir build-host -R abo_host_wire_tests --output-on-failure
  ```

  Expected: FAIL，因为当前 ACK 只有 `cmd/tx`，state 不暴露已提交曲目。

- [x] **Step 2: 实现最小消息格式器并接入 HostLink。**

  `HostLink` 继续只在 cJSON 边界解析输入；提交成功后发送带曲目信息的 ACK，再发送一份 state。不得把浏览器状态作为板端真相，不得修改现有 CRC 字节规范。

  Expected: `abo_host_wire_tests` 和 `abo_score_protocol_tests` PASS。

- [x] **Step 3: 写串口 HIL 脚本，不引入新依赖。**

  `verify_score_link.py` 使用 ESP-IDF 现有 venv 中的 pyserial，依次执行：ping→hello/state；提交固定 `hil-scale`；确认 state 的 `score_id`；发送第二事务的坏 CRC；确认 `crc_mismatch` 且 state 仍是 `hil-scale`。脚本必须在 finally 中关闭端口。

- [x] **Step 4: ASCII 构建并停在烧录批准点。**

  使用新目录 `C:\esp-work\abo-link-closeout-fw`，不得覆盖 `C:\esp-work\abo-independent-fw`。装配环境并构建：

  ```powershell
  . C:\esp-work\tools\idf-env.ps1
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" --version
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B build -DABO_PRODUCT=ON build
  ```

  Expected: ESP-IDF v5.5.5；镜像小于 2.4MiB。记录大小与 SHA-256，然后停止并向用户申请烧录。

- [x] **Step 5: 用户批准后做 T03-U 终验。**

  用户关闭 Web Serial/monitor，短按一次 BOOT；Luna 用 `Get-PnpDevice -PresentOnly` 核实实际 `VID_303A&PID_1001` 和 COM。只运行标准 `idf.py -p COMx flash`。正常关机开机后：

  ```powershell
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" tools\verify_score_link.py --port COMx
  ```

  再用 `console/index.html` 手工确认连接、装载、拔线清理和重新连接。全部通过后才把 T03-U 卡状态改为完成；若失败，只修链路，不进入 Task 1。

---

## Task 1: 先校正内存合同与资产归属

**Files:**

- Modify: `docs/技术方案.md`
- Modify: `flow/decisions.md`
- Create: `firmware/course-motherboard/abo_assets/delivery/ATTRIBUTION.md`
- Modify: `firmware/course-motherboard/abo_assets/tests/test_p1_asset_manifest.py`

- [x] **Step 1: 把实测容量写成 RED 资产门。**

  扩展 `test_p1_asset_manifest.py`，断言总 EIAD 为 773134B；各乐器 PCM 字节数分别为 1402514/1008000/604800；最大单乐器缓存为 1402514B 且小于 2MiB；禁止测试假设钢琴+小提琴可同时常驻。

- [x] **Step 2: 更新合同，不改变 23 枚声音内容。**

  将 `docs/技术方案.md` 的“当前乐器＋下一乐器完整预取”改为“单一固定 PCM 仓；静音帧边界原地换仓；压缩资产全部留在 Flash”。在 `flow/decisions.md` 记录原因与否决方案。不得改 `manifest.json` 中的音频哈希、循环点和大小。

- [x] **Step 3: 补齐交付归属文件。**

  `delivery/ATTRIBUTION.md` 必须列出：FreePats Upright Piano KW（CC0 1.0）、VSCO-2-CE Solo Violin（CC0 1.0）、`nbrosowsky/tonejs-instruments` 单簧管（项目级 CC BY 3.0，原始作者未核实），并指向 `manifest.json` 的逐文件 SHA-256。测试断言文件存在并含这三项和风险警示。

  Run:

  ```powershell
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" abo_assets\tests\test_p1_asset_manifest.py
  ```

  Expected: PASS；无 EIAD 文件发生哈希变化。

---

## Task 2: 建立 23 根音的嵌入式 SoundBank

**Files:**

- Create: `firmware/course-motherboard/features/speaker_assets/include/speaker_assets/abo_p1_sound_bank.h`
- Create: `firmware/course-motherboard/features/speaker_assets/abo_p1_sound_bank.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p1_sound_bank_contract_tests.cpp`
- Modify: `firmware/course-motherboard/features/speaker_assets/CMakeLists.txt`
- Modify: `firmware/course-motherboard/host_test/abo_p1_asset_decode_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: 写 23 根元数据 RED 测试。**

  测试固定三张根音表及 MIDI：钢琴 `47/54/60/66/72/78/84`；小提琴 `55/60/64/67/72/76/81`；单簧管 `50/53/58/62/65/70/74/77/82`。逐项断言 encoded 非空、样本数、循环起止与 manifest 相同。

- [x] **Step 2: 定义无动态分配的只读资产表。**

  `AboP1EncodedRoot` 至少包含 `id/root_midi/encoded/encoded_bytes/decoded_samples/loop_start_sample/loop_end_sample`；`AboP1EncodedBank` 包含 `instrument_index/roots/root_count/decoded_samples_total`。公开 `abo_p1_sound_bank(0..2)` 和 `abo_p1_piano_c4_sound()` 兼容启动音。

- [x] **Step 3: 在 ABO_PRODUCT 分支嵌入全部 23 枚 EIAD。**

  CMake 只引用 `abo_assets/delivery` 的正式 23 枚，不嵌入历史 flute、preview、WaytoAGI 或 incoming 文件。链接后检查新增字节约为当前 C4 之外的 708920B。

- [x] **Step 4: 将解码测试扩为全部 23 枚。**

  `abo_p1_asset_decode_tests` 逐文件打开、完整解码并核对采样率 48000、总样本数和循环范围；不得只检查 C4。

  Expected: 资产契约、SoundBank 合同、23 文件解码全部 PASS。

---

## Task 3: 消除主任务与音频任务的数据竞争，并支持平滑重触发

**Files:**

- Create: `firmware/course-motherboard/abo_p1/include/abo_p1/voice_command_queue.h`
- Create: `firmware/course-motherboard/abo_p1/voice_command_queue.cpp`
- Create: `firmware/course-motherboard/abo_p1/include/abo_p1/mono_voice_mixer.h`
- Create: `firmware/course-motherboard/abo_p1/mono_voice_mixer.cpp`
- Create: `firmware/course-motherboard/host_test/abo_voice_command_queue_tests.cpp`
- Create: `firmware/course-motherboard/host_test/abo_mono_voice_mixer_tests.cpp`
- Modify: `firmware/course-motherboard/abo_p1/include/abo_p1/voice_engine.h`
- Modify: `firmware/course-motherboard/abo_p1/voice_engine.cpp`
- Modify: `firmware/course-motherboard/abo_p1/include/abo_p1/voice_renderer.h`
- Modify: `firmware/course-motherboard/abo_p1/voice_renderer.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`

- [x] **Step 1: 为命令队列写 RED。**

  固定容量 16；单生产者是 app/main，单消费者是音频 provider。覆盖 press/release 顺序、队满拒绝、generation、旧按键 release 不得停止新音符、清空和 drop 计数。队列不得分配堆内存。

- [x] **Step 2: 为双声部重触发写 RED。**

  覆盖：初次 note-on；同键重按；不同键重触发；旧键松开不截断新音；10ms（480 samples）互补交叉淡化期间混音不溢出；普通松键采用乐器配置的 release；Idle 始终输出零但 provider 不制造帧间隙。

- [x] **Step 3: 最小实现命令队列和 MonoVoiceMixer。**

  所有 `VoiceRenderer` 状态只能由音频消费者线程修改。主任务只入队。混音使用 32-bit 中间值并饱和到 PCM16；保留已通过的 64-bit Q16.16 源游标和 32-bit phase step。

- [x] **Step 4: 跑声音核心回归。**

  ```powershell
  cmake --build build-host --target abo_voice_engine_tests abo_voice_renderer_tests abo_voice_command_queue_tests abo_mono_voice_mixer_tests
  ctest --test-dir build-host -R "abo_voice|abo_mono" --output-on-failure
  ```

  Expected: 所有声音核心测试 PASS，正式 `[63360,125257)` C4 长游标回归仍在。

---

## Task 4: 用固定 PSRAM 仓替换单根 PianoVoiceSession

**Files:**

- Create: `firmware/course-motherboard/main/platform/instrument_voice_session.h`
- Create: `firmware/course-motherboard/main/platform/instrument_voice_session.cpp`
- Create: `firmware/course-motherboard/host_test/abo_instrument_voice_contract_tests.cpp`
- Modify: `firmware/course-motherboard/main/CMakeLists.txt`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`
- Modify: `firmware/course-motherboard/main/abo_app_main.cpp`
- Retain uncompiled for history: `firmware/course-motherboard/main/platform/piano_voice_session.h`
- Retain uncompiled for history: `firmware/course-motherboard/main/platform/piano_voice_session.cpp`

- [x] **Step 1: 写平台合同 RED。**

  断言新 session：一次只分配 `701257` 个 PCM16 样本（1402514B）；最多 9 个 root 描述；完整解码当前 bank；provider 空闲时持续返回静音帧；切仓前必须收到音频线程 `bank_safe`；切仓失败时重新装载上一 bank 并报告错误。

  Evidence: 初次运行新合同测试按预期 RED；实现后 `abo_instrument_voice_contract_tests.exe` PASS。

- [x] **Step 2: 实现固定仓和安全换仓握手。**

  启动时只申请一次 `MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT` arena。音频线程看到换仓请求后在 10ms 帧边界释放当前声部、进入只写零状态并发布 `bank_safe`；主任务收到后才能覆盖 arena。发布新 roots 后音频线程恢复。严禁在 provider/I2S 线程里解码或动态分配。

  Evidence: `InstrumentVoiceSession` 使用一次 1402514B PSRAM arena；解码仅在主任务换仓路径；产品源表不再编译 `piano_voice_session.cpp`。

- [x] **Step 3: 让 voice stream 覆盖整个演奏会话。**

  启动音播放结束后进入 P1 测试会话，`SpeakerOutput::request_voice_stream` 只申请一次；无按键时输出零，不结束流。退出会话才令 provider 在 release 完成后返回 `finished=true`。继续复用 `AudioIoArbiter`、GPIO8 电源租约和既有 I2S 驱动。

- [x] **Step 4: 接入三套 bank，并保留启动 C4。**

  默认 bank 为钢琴。启动提示仍直接播放正式 `piano_c4.eiad`；之后加载完整钢琴 bank。切换期间 HostLink 状态必须可表达 `instrument_loading=true`，加载完成后才修改实际 instrument。

  Expected: 平台合同测试 PASS；`PianoVoiceSession` 不再出现在 ABO_PRODUCT 编译源表或 `abo_app_main.cpp`。

  Evidence: ESP-IDF 5.5.5 在 `C:\esp-work\abo-p1-v3-fw` 构建通过；app binary 1097104B（3MiB factory 余量 0x1f4270）；未烧录、未执行 `erase_flash`。

---

## Task 5: 接入 21 音、S8 与旋钮

**Files:**

- Create: `firmware/course-motherboard/abo_p1/include/abo_p1/performance_controls.h`
- Create: `firmware/course-motherboard/abo_p1/performance_controls.cpp`
- Create: `firmware/course-motherboard/host_test/abo_p1_performance_controls_tests.cpp`
- Modify: `firmware/course-motherboard/main/abo_app_main.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/host_link.h`
- Modify: `firmware/course-motherboard/abo_host/host_link.cpp`
- Modify: `firmware/course-motherboard/abo_host/include/abo_host/wire_messages.h`
- Modify: `firmware/course-motherboard/abo_host/wire_messages.cpp`
- Modify: `firmware/course-motherboard/host_test/CMakeLists.txt`
- Modify: `firmware/course-motherboard/host_test/abo_host_wire_tests.cpp`
- Create: `firmware/course-motherboard/host_test/abo_host_score_protocol_core.cpp`
- Create: `firmware/course-motherboard/host_test/abo_host_wire_messages_core.cpp`

- [x] **Step 1: 写控制语义 RED。**

  S1--S7 映射唱名 `0..6`；音区为 C3--B3/C4--B4/C5--B5；旋钮每 detent 在 3→4→5→3 或反向循环，且只影响下一 note-on；S8 按下按钢琴→小提琴→单簧管→钢琴循环，release 不重复切换。编码器按压暂不启动 P2 行为。

  Evidence: `abo_p1_performance_controls_tests` 覆盖按下沿、匹配 release、重复按压抑制、三音区正反向环回和 45° 旋钮角度。

- [x] **Step 2: 写 63 组合根音选择 RED。**

  对三乐器 × 三音区 × 七唱名逐一调用 `VoiceEngine::note_on`，断言 target MIDI、选中 root、phase step。钢琴与单簧管位移绝对值不超过 3；小提琴 G3--B5 不超过 2，C3--F#3 只按已记录的低音例外验音高，不伪称真实小提琴音域。

  Evidence: 63 组合循环全部通过目标 MIDI、最近根音距离与非零 Q16.16 phase step 断言。

- [x] **Step 3: 实现输入到命令队列。**

  S1--S7 press 入队 note-on；free/P1 测试态的 matching release 入队 note-off。S8 若当前有声，先排队 release，再进入 bank loading；加载期间新的 note-on 明确拒绝并通过 state 回报，不静默吞键。

  Evidence: `InstrumentVoiceSession` 的主任务接口只入队，音频 provider 独占消费 `VoiceCommandQueue`；S8 释放已持有键后再请求 bank，加载状态通过 state 回传。

- [x] **Step 4: state 改为实际快照。**

  去掉 `HostLink::send_state()` 中硬编码的 mode/instrument/octave。P1 至少回传实际 instrument、octave、keys、loading、volume 默认值和已装载 score；网页只负责显示。

  Expected: 63 映射测试、控制测试、HostLink wire 测试全部 PASS。

  Evidence: wire state 新增 `instrument_loading` 与 `volume`；`abo_p1_performance_controls_tests`、`abo_host_wire_tests`、`abo_instrument_voice_contract_tests` 及 Task 3 的四个声音核心目标均 PASS。ESP-IDF 5.5.5 产品构建同步通过，app binary 1099344B，3MiB factory 余量 0x1f39b0。

---

## Task 6: 全回归、ASCII 构建与 P1 真机验收

**Files:**

- Build only: `C:\esp-work\abo-p1-v3-fw\`
- Update after evidence: `flow/进展.md`
- Update on failures: `flow/踩坑记录.md`

- [x] **Step 1: 运行资产、网页和全量宿主回归。**

  ```powershell
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" abo_assets\tests\test_p1_asset_manifest.py
  node console\demo-p1-voice.test.mjs
  cmake --build build-host
  ctest --test-dir build-host --output-on-failure
  ```

  Evidence 2026-09-03: 资产 manifest `23 assets / 773134 bytes` PASS，网页映射测试 PASS；ASCII 宿主构建 `223/223` 编译步骤完成，70 个测试逐进程执行 `70/70 PASS`。本机 CTest 子进程仍会偶发 `0xc0000135`，因此不能把该工具层现象当作断言回归；复验命令与原因见 `flow/踩坑记录.md`。

- [x] **Step 2: 复制到全新的 ASCII 目录并构建。**

  源为 `firmware/course-motherboard`，目标为 `C:\esp-work\abo-p1-v3-fw`。先用只读检查确认目标不存在或为空，再用 `robocopy /E` 复制；不使用会删除目标额外文件的镜像参数。随后 dot-source `idf-env.ps1` 并运行 `idf.py -B build -DABO_PRODUCT=ON build`。

  Evidence 2026-09-03: `C:\esp-work\abo-p1-v3-fw` 使用 ESP-IDF 5.5.5 构建通过；app `1099376B`，3MiB factory 余量 `0x1f3990`；map 唯一 EIAD 符号 `23/23`；bin SHA-256 `52B52847068EE95B357B7483CBABFFF28D24A621FD21C85079C3A2AA4BB5CE72`。本轮未烧录。

- [x] **Step 3: 记录 PSRAM 和切仓指标。**

  已在 `InstrumentVoiceSession` 增加 `bank_begin`/`bank_swap` 诊断日志：输出 `bank_load_ms`、`arena_bytes`、`free_psram`、`largest_psram` 和结果码；合同测试已 RED→GREEN，固件构建通过。2026-09-03 真机读取 `bank_begin=457ms`，并完成钢琴→小提琴→单簧管→钢琴三次切仓：`330ms/198ms/458ms`，均 `result=0`、均低于 500ms，固定仓与 PSRAM 数值稳定。

- [x] **Step 4: 停止并申请烧录批准。**

  Evidence 2026-09-03: 用户明确批准后，识别 `VID_303A&PID_1001 / COM5`；标准三段写入 bootloader、partition-table、app 均 `Hash of data verified`，未执行 `erase_flash`。当前待正常电源重启退出下载模式。

- [ ] **Step 5: 真机 63 组合矩阵。**

  每件乐器依次在低/中/高音区短按 S1--S7，共 63 个短音；每个音区另长按 S1/S4/S7 至少 4 秒，共 27 个长音；每轮检查松键停止、无重播间隔、无崩溃。检查 S8 三次闭环、旋钮正反向三音区闭环、切换只影响下一次 note-on、网页显示与实体一致。

- [ ] **Step 6: P1 停止门。**

  只有 63 个组合全部有声、三乐器可辨、按键延迟 <50ms、切仓 <500ms、app/PSRAM 门槛通过，才在 `flow/进展.md` 标记 T03/P1 完成并进入 P2。小提琴的自然颤音和单簧管已接受的高音颤动只记录为后续音色优化，不在本轮反复换源。

## Final Self-Review

- [ ] 搜索并清除实现中的占位符与过期单根假设：`rg -n "TODO|TBD|PianoVoiceSession|RequestPianoC4" firmware/course-motherboard`。
- [ ] 核对 23 资产哈希未变、C4 64-bit 游标回归仍通过、所有真实音频访问只发生在音频消费者线程或换仓安全态。
- [ ] 确认没有新增裸 GPIO8/I2S 路径，没有覆盖当前回退镜像，没有执行 Flash 全盘擦除。
- [ ] 由非 Luna 模型复审代码、内存预算和测试证据后，再向用户宣称 P1 完成。
