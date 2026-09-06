# T03--T06 · Luna 后续执行入口

> 状态：P1 已完成 63 音与 S8 切仓真机验收；P2 Task 1/2/3/4 与 Task 5 Step 1/2 已完成。“暂停 = Standby”候选已完成测试、构建并标准烧录 COM5，正常重启后的五灯与钢琴 C4 已通过；P3 Task 0/1/2、网页 Task 4 RED→GREEN、谱面固定中央指针网页 HIL 与 T06 最终整机主流程已完成，当前进入 C7/C8 证据归档与最终交接 · 2026-09-06

## 当前交接覆盖事实

- 用户已接受当前 P1 的功能状态：钢琴、小提琴三个音区正常；单簧管三个音区均能发声，但音质不够完美，属于已知非阻断问题。
- 当前不修改单簧管 9 枚 PCM16LE 资产，不修改固件，不重新构建，不烧录；后续优化另开专项。
- P2 Task 1 已完成：纯 C++ `PerformanceController` 已覆盖跟谱/自由模式、严格判定、暂停/恢复和结束态；RED→GREEN 宿主测试通过。
- P2 Task 2 已完成：`PerformanceClock` 以 64 位分子/余数累加实现 frame→tick，覆盖 BPM、不同帧块和长时间守恒；控制器已在持音阶段使用该时钟，等待/自由阶段只累计用时。不改音频、不烧录。
- P2 Task 3 已完成：`RhythmLedModel` 固定 D1--D5 的节拍蓝点、正确绿脉冲、错误 120ms 红闪、暂停低亮琥珀和曲终一次彩色往返；`StatusLedStrip::show_rhythm_frame` 在 P0 冷启动序列 active 时拒绝覆盖，结束后复用既有 RMT/WS2812 平台入口。
- P2 Task 4 已完成：`ScoreSnapshot` 在待机态复制已提交乐谱；P2 runtime 成为 app 的动作边界，统一接入 P1 声音会话、bank-safe 换仓、LED 和 HostLink；I2S provider 只用原子 frame counter 回报，主循环推进状态机；HostLink state/judge/result 已补齐控制台消费的 P2 字段。新增同 ID 内容变化的 CRC 识别，避免待机时误跳过新乐谱。
- P2 Task 4 证据：聚焦回归 `12/12 PASS`（4 项 P2 CTest + 8 项相关 P1/协议目标在正确 MinGW PATH 下直跑），覆盖 P2 控制器/时钟/LED/平台合同、score protocol、HostLink wire、P1 声音与会话回归；本 Task 未执行 ESP-IDF 构建、未烧录、未执行 `erase_flash`。全量 CTest 尚未宣称通过，旧中文/`#` 路径外部源对象目录问题仍留给纯 ASCII 的 P2 Task 5 处理。
- P2 Task 5 Step 1 已完成：源代码复制到全新 `C:\esp-work\abo-p2-fw`，新建 `host_test\build-task5` 完成全量 CTest `74/74 PASS`；使用 ESP-IDF `v5.5.5-dirty`、`ABO_PRODUCT=ON` 完成 ESP32-S3 产品构建。应用 BIN `1,642,560B (0x191040)`，factory 余量 `0x16efc0`（`1,503,168B`，`47.78%`），SHA-256 `986B65139C91F0C21B81C8677915D0330E23F3EE52D387EAF7FDCC836B36479F`，固定 PSRAM 解码仓 `1,402,514B`；最低 free PSRAM 需 Step 2 真机 HIL 采集。未烧录、未执行 `erase_flash`。
- P2 Task 5 Step 2 烧录已完成：首次候选造成旋钮一次点击后进入 `score/paused/cursor=0/errors=0`；修复后用户再次短按 BOOT，当前态确认 `USB\VID_303A&PID_1001 / COM5`，使用 `C:\esp-work\abo-p2-fw-encoder-fix` 标准 flash，bootloader、partition table、app 均 `Hash of data verified` 并硬复位；未执行 `erase_flash`。当前等待正常电源重启后复验修复 HIL。

- 后续 HIL 已确认：旋钮单击可进入 `free/playing`，钢琴/小提琴/单簧管共 63 个目标音与 S8 三乐器循环正常；旧固件从 Playing 只能进入独立 `paused`，而 `set_mode` 只接受 Standby，导致暂停后网页无法切换模式。
- 用户已冻结“暂停 = Standby”：Playing 按旋钮立即停音并进入 Standby；同模式再按旋钮继续，网页在 Standby 切换模式时清零旧模式本次进度。修复镜像位于 `C:\esp-work\abo-p2-standby\build-standby\easy_input_keyboard.bin`，`1,643,008B (0x191200)`，SHA-256 `F837A733EC5D1CBA7D81D2A71B2D724C18750BDC4F895600879114108DC50D51`；ASCII 宿主 `76/76` 与 ESP-IDF 构建通过，已在 PresentOnly `VID_303A&PID_1001 / COM5` 下标准烧录并三段校验通过；用户正常重启后确认五灯与钢琴 C4，待 Web Serial 功能 HIL。
- P3 网页 Task 4 RED→GREEN 已完成：`console/index.html` 接入 state/input 校验、音量 `set_volume` 事务、S9 双视图真实输入反馈、曲终自定义 modal、完整 42 音共用渲染器/按拍滚动与导入帮助；全部 16 个 `console/*.test.mjs` 通过，嵌入脚本解析通过。未构建、未烧录。
- 谱面固定中央指针网页任务已完成：五线谱/简谱只移动当前活动轨道，使用 `translate3d` 平滑跟随板端 `cursor/subphase/note_ticks/note_total_ticks`；权威 `standby + cursor=0`、暂停、切谱与导入复位均已由用户在 COM5 确认正常。全量 console 回归 16/16，内嵌脚本解析 2/2；未改固件、未烧录。
- T06 最终整机主流程已由用户确认“没有问题”：冷启动、USB-Serial/JTAG、连接/装载/首次发声、跟谱/自由、音量、S8、旋钮开始暂停继续、曲终复位与拔线重连均未发现阻断问题；当前不再进入功能修改分支。

## C7/C8 最终证据归档

- C7 方法论证据索引已归档至 `docs/最终交接.md` §1：老师方法论与本项目 P0'→P1'→P2'→P3' 路线、板端/网页职责边界和 USB-Serial/JTAG 选择均可由 PRD §01/§09、`flow/plan.md`、`docs/技术方案.md` 复核。
- C8 思考过程证据索引已归档至 `docs/最终交接.md` §2：13 项 PRD 决策、`flow/decisions.md` 决策日志、`flow/进展.md` 六字段交接和 `flow/踩坑记录.md` 根因记录形成闭环。
- T06 功能摘要与回退边界见 `docs/最终交接.md` §3/§4；当前版本冻结，单簧管音质不完美明确为非阻断后续专项。

## 目标

让后续 Luna 从当前“单根钢琴 C4 真机闭环”继续，按契约完成剩余串口门、P1 63 组合声音、P2 双模式状态机、P3 Web 控制台和 T06 最终验收；不得把这些阶段揉成一次不可定位的大改。

## 必读与执行顺序

Luna 每次会话先按根 `AGENTS.md` 的 1--6 阅读，再读本卡。实施必须按下列顺序：

1. `docs/superpowers/plans/2026-09-03-p1-63-voice-expansion.md`
   - 先完成 Task 0 的 T03-U/T05-B 遗留事务真机门。
   - 再完成 23 根音、固定 PSRAM 仓、63 组合、S8、旋钮和 P1 HIL。
2. `docs/superpowers/plans/2026-09-03-p2-follow-free-state-machine.md`
   - 先纯 C++ 状态机/时钟/LED，再接平台和 P2 HIL。
3. `docs/superpowers/plans/2026-09-03-p3-console-and-final-hil.md`
   - 冻结控制消息、接网页、全回归、最终构建与 T06 HIL。

前一份计划的停止门没有通过，禁止开始后一份。

## 当前事实

- 真机已通过单根 C4：五灯、启动音、S1 短音、长按至少 4 秒连续、松键淡出。
- 当前回退镜像：`C:\esp-work\abo-independent-fw\build\easy_input_keyboard.bin`，382672B，SHA-256 `DF1022F009DDDB30C2E6CEC75723E9799971E4BC6CDACD3A5720A5073D932447`。首次构建新阶段前先复制保存，不覆盖原文件。
- 当前正式资产已为 23 根、`1,309,174B` 混合 payload（钢琴/小提琴 14 根 EIAD + 单簧管 9 根 PCM16LE）；63 指 21 个目标音 × 3 乐器，不是 63 个完整采样。
- 单簧管 9 枚正式 `.pcm16le` 已与 Task 1 候选 PCM WAV data 逐字节一致；旧 `.eiad` 文件保留作回溯，但正式 manifest 与 ABO_PRODUCT CMake 已不再引用，固件加载分支已完成。
- 全部 PCM 同时解码为 3015314B；钢琴+小提琴为 2410514B，超过 2MiB 预算。推荐用一个 1402514B 固定 PSRAM 仓原地换乐器。
- 默认不删根音；只有 app >2.4MiB 或单簧管切仓 >500ms 时才评审删除单簧管 D4/D5，且先做 A/B 与更新合同。
- 小提琴自然颤音保持当前已通过资产；单簧管不换源、不调增益，已用④同处理的连续 PCM 与④ EIAD 完成九根音 A/B 停止门，避免再次进入换源/codec 反复循环。
- Task 6 宿主证据：资产 manifest `23/23`、网页映射 PASS、ASCII 宿主测试 `70/70 PASS`；ESP-IDF 5.5.5 app `1099376B`，factory 余量 `0x1f3990`，最终 bin SHA-256 `52B52847068EE95B357B7483CBABFFF28D24A621FD21C85079C3A2AA4BB5CE72`，路径 `C:\esp-work\abo-p1-v3-fw\build\easy_input_keyboard.bin`。
- 已加入 `InstrumentVoiceSession` 的 `bank_begin`/`bank_swap` 诊断日志（加载毫秒数、固定 arena 字节数、free PSRAM、最大连续块、结果码）；尚未取得真机启动与三次切仓指标。
- 最终 P1 固件已按批准写入 COM5，bootloader、partition-table、app 均返回 `Hash of data verified`；用户随后已正常开机并完成基础声音验收：钢琴、小提琴三个音区正常，单簧管三个音区可发声但音质问题延后。
- 已完成正常开机监视：PID `1001/COM5` 为最终 USB-Serial/JTAG 运行态，收到 `hello/state`；`CONFIG_LOG_DEFAULT_LEVEL_WARN` 过滤新增 INFO 诊断行的事实保留为资源指标遗留项，不阻挡当前进入 P2，但若后续验收 PSRAM/切仓耗时仍需单独修正日志并重新烧录。
- 根目录没有 Git 元数据；不要要求 commit，不要把无 Git 当环境故障。

## Task 0/Task 1/Task 2/Task 3 已确认的声音路线（2026-09-03）

- 保留 23 枚根音、63 个可弹目标、单簧管九根音映射及④的 0.8 秒窗口/逐根循环终点/20ms 交叉淡化。
- 钢琴与小提琴继续 EIAD-v1；单簧管正式目标为连续 PCM16LE，运行时仍复用同一个 `1,402,514B` 固定 PSRAM 仓。
- Task 1 已通过：候选包 `delivery/clarinet-tonejs-pcm-preview/` 的九根音单次/循环 A/B 均通过，且 PCM 优于④ EIAD。
- Task 2 已通过：正式 `delivery/manifest.json` 与 9 个 `.pcm16le` 已更新。
- Task 3 已通过：CMake 已嵌入 14 个 EIAD 与 9 个 PCM16LE；`AboP1RootStorage` 与 `InstrumentVoiceSession` 双加载分支已完成；四个目标宿主测试通过。
- Task 4 已通过：纯 ASCII 副本 `C:\esp-work\abo-p1-clarinet-pcm-fw` 中资产测试 `7/7`、网页测试 `16/16`、宿主 CTest `70/70`；ESP-IDF `v5.5.5-dirty` 构建退出码 `0`，app `1,635,856B`，factory 余量 `0x1709f0`（48%），map 资产为 `14 EIAD + 9 PCM16LE`，候选 bin SHA-256 为 `F393B3D84D3EA39DE3A31D59C1A65FBAFBBA0E903DE693971D07EE382C2A520A`。
- Task 5 已完成烧录与当前功能验收：下载态为 `USB\VID_303A&PID_1001 / COM5`；使用 Task 4 候选镜像标准 flash，三段均 `Hash of data verified`，`FLASH_EXIT=0`；未执行 `erase_flash`。用户已正常重启并确认钢琴、小提琴三音区正常，单簧管三音区可用但音质不完美。
- 当前下一步为 P2 Task 5；不再围绕单簧管音质做迭代。P2 Task 4 仅完成宿主平台接入与聚焦回归，未进行 ESP-IDF 固件构建或烧录；Task 5 需在纯 ASCII 副本中完成全量构建和 P2 真机 HIL，并另行取得烧录批准。

## 给 Luna 的启动提示词

```text
按 AGENTS.md 开工前必读 1--6 阅读，然后读 flow/tasks/T03-T06-Luna执行.md 和其中指向的当前阶段计划。使用 superpowers:executing-plans，从尚未勾选的第一个 Task 开始，一次只执行一个 Task；先 RED 后 GREEN。修改前先向我确认本 Task 方案，烧录前再次单独确认。E: 只作真相源，ESP-IDF 构建复制到计划指定的全新 C:\esp-work 纯 ASCII 目录；不要重装、不要 eim、不要全盘擦除 Flash。完成后按 project-flow-cy 操作 B 写六字段交接并停止，不自动跨越阶段停止门。
```

## 每一 Task 的完成证据

1. 修改文件清单及原因。
2. RED 的实际失败与 GREEN 的实际通过。
3. 全量回归测试数，不只报新增测试。
4. 涉及构建时记录 ESP-IDF 版本、bin 大小、SHA-256、factory 余量与 PSRAM。
5. 涉及真机时记录 PresentOnly VID/PID/COM、用户观察与是否执行标准 flash。
6. 六字段 `flow/进展.md` 交接条；若改变方向同时更新 `flow/plan.md`/`flow/decisions.md`。

## 必须停止并询问用户的情况

- 需要修改 23 枚正式音频、删除根音、改变乐器或改变协议字段。
- 需要烧录、写声音分区、覆盖回退镜像或新增外部下载。
- app ≥2.4MiB、PSRAM 峰值 ≥2MiB、切仓 ≥500ms、按键到声音 ≥50ms。
- 自动测试未全绿，或真机结果与网页/宿主测试不一致。
- 目标路径、COM 或设备 PresentOnly 身份不明确。

## 明确禁止

- 不启动 HID/BLE/EasyInput App，不把课程快捷键与乐队键语义并行。
- 不在 E: 含中文/`#` 路径执行 ESP-IDF 构建。
- 不重装 ESP-IDF、不新建 venv、不运行 `eim install/fix`。
- 不对 Flash 做全盘擦除，不写 `sound_a`/`sound_b`。
- 不让网页承担声音、判定或节拍时间轴。
- 不跳过用户确认、不连续自动烧录、不因听感问题陷入无止境换源。
