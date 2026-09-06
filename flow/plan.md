# 计划 (plan) —— 契约

> 经确认后执行。要偏离，**先改这里**再动手。

## 里程碑
- [x] M1：需求收敛与 PRD（含可玩原型）—— 2026-09-01 完成
- [x] M2：技术方案冻结（音色实现路线、乐谱 JSON 协议、P0'-P3' 实施计划）—— 2026-09-01 完成
- [x] M3：固件 + 控制台实现，烧录真机
- [x] M4：15 秒主流程验收，收工交接

## 任务拆解
| 任务 | 负责角色 / 工具 | 输入 | 产出(落哪个文件) | 验收标准 |
|---|---|---|---|---|
| T01 技术方案与实施计划（已完成） | TRAE | PRD + 老师源码本地副本 | docs/技术方案.md | 音色路线定案有依据；谱面 JSON 字段冻结；每阶段有真机验收标准；复用边界明确 |
| T02 P0' 课程母本内安全上电自检（历史阶段，已完成） | TRAE + 用户真机 | 已恢复的课程母本 | `docs/superpowers/plans/2026-09-01-p0-motherboard-integration.md` + flow/进展.md | 当时保留 HID/BLE/App 并证明母本可恢复；冷启动由既有 GPIO8 时序完成五灯 + 扬声器启动声，之后归还 LED/I2S |
| T03-U 独立固件 USB-Serial/JTAG 基线 | TRAE + 用户真机 | T02 已验证硬件层 + 老师 host link 方法 | `docs/superpowers/plans/2026-09-02-independent-usb-serial-and-score-link.md` + firmware/ + console/ | 正常开机出现串口；`ping → hello/state` 通过；无 HID/BLE/App；GPIO8、五灯、C4 启动音与按键扫描不回归；乐谱事务正常/坏 CRC 均可验 |
| T03 P1' 声音引擎 | TRAE | T01 方案 | firmware/（ASCII 路径编译） | 21 音 × 3 音色单音可弹，旋钮调音区生效 |
| T04 P2' 双模式状态机 | TRAE | T03 | firmware/ | 严格判定、卡住时间轴暂停、旋钮语义、D1-D5 光效 |
| T05-A P3' 控制台功能与视觉框架（已完成） | TRAE + UI agent | PRD / prototype.js | docs/Web控制台功能框架.md + console/index.html | 三个工作区、曲目选择、键盘还原、谱面、状态与音量框架可用；板端仍为状态真相 |
| T05-B Web 协议加固与 T03-U 联调 | TRAE | T03-U + T05-A | console/index.html + Web 宿主测试 | 串口清理、握手、完整 schema 校验、规范 CRC、上传 ACK/超时/回滚通过；拔线无幽灵状态 |
| T05-C P3' 完整控制台联调 | TRAE | T04 + T05-B | console/（单文件 HTML） | Standby 模式切换、音量与曲终复位请求接入；实体旋钮负责开始/暂停/继续；红蓝键、游标、成绩和板端状态一致 |
| T06 真机验收与交接 | TRAE + 用户 | 全部 | flow/进展.md | 15 秒主流程全链路通过，C7/C8 证据齐 |

## T03 已确认执行边界（2026-09-01）

1. 最终 A Band of One 固件采用独占的“乐队模式”：S1--S7 只演奏、S8 只切乐器、旋钮只控制产品语义；该模式不发送第二课原快捷键 HID。
2. ~~保留 USB/BLE 和 EasyInput App 的连接能力。~~ 此条已由下方 2026-09-02 独立固件修订覆盖；保留在此仅说明历史决策。
3. 先完成 23 枚源根音（钢琴 7 + 同源小提琴 7 + 单簧管 9）离线资产管线与宿主测试，再接入课程母本已有 `SpeakerOutput` / `AudioIoArbiter`；仍只输出 21 音 × 3 乐器的 63 个可弹组合，禁止另起裸 GPIO8/I2S 音频路径。
4. P1' 每小步顺序：资产 RED/GREEN → 单根音板端发声 → 21 音变调 → 三音色/S8/音区 → 真机验收。未经用户确认不得烧录，永不执行 `erase_flash`。

## T03 修订：音源路径改为声部模型（2026-09-01）

KEY8 探针真机复验暴露课程 `SpeakerOutput`（一次性播放器）不适合持续音，循环补丁修不干净（详见 decisions/踩坑记录 2026-09-01 条目）。T03 音源路径修订如下，其余边界不变：

- **架构**：新增 `abo_p1::VoiceEngine`（纯 C++ 逻辑层，宿主可测）+ 渲染泵（平台层）。声部状态机 Idle→起音→持续(循环区)→释放(淡出)→Idle；相位累加 Q16.16 + 线性插值变调；钢琴 7 根音、同源小提琴 7 根音 `G3/C4/E4/G4/C5/E5/A5`、单簧管 9 根音，共 23 枚源采样。小提琴 G3--B5 的最近根音变调不超过 ±2 半音，C3--F#3 为乐器音域下限造成的向下模拟例外。PSRAM 只分配一个按最大 bank（1,402,514B）确定的固定 PCM 仓，在静音帧边界原地换入下一乐器；演奏会话内 I2S 连续 DMA 永不停机。生命周期挂现有 GPIO8 电源仲裁（首个 note_on 启动、全静音释放）。
- **与课程代码关系**：`SpeakerOutput` 保留用于启动音/诊断（一次性路径与契约测试不动）；禁止裸 GPIO8/I2S 的边界不变——渲染泵复用既有 I2S/电源仲裁，只是不经过一次性播放请求链路。
- **切片顺序**：V1 纯逻辑宿主测试全绿（状态机/变调频率/循环回绕无爆点/淡出/PSRAM 预算）→ V2 KEY8 真机验收（长按无缝持续、短按急促短音，即点音 bug 终验）→ V3 63 组合（8 键×3 乐器×3 音区+S8+旋钮，P1' 收尾）。
- **资产优化同步**：先用 `console/demo-rootmap-7x3.html` 验证钢琴 7 根音映射；弦乐采用已经网页验收的 VSCO Solo Violin `Arco Vib f` 同源 7 根音 `G3/C4/E4/G4/C5/E5/A5`，不再采用三种提琴分区。单簧管采用已核验的 9 根音 `D3/F3/A#3/D4/F4/A#4/D5/F5/A#5`，使 C3--B5 的最大变调保持 −2 至 +3 半音。该来源按用户选择接受上游项目级 CC BY 3.0 样本声明，转换产物必须附带 `ATTRIBUTION.md` 并标注“原始作者未核实”。循环交叉淡化以离线烘焙实现；钢琴采用原始 SFZ 的循环/非循环分界，不再冻结统一的 [1.2,1.95)s 窗口。裁循环尾的 Flash/PSRAM收益在 23 枚源采样清单确定后重新测算。V2 前按 Task 0/Task 1 停止门生成 14 个 EIAD 与 9 个 PCM16LE。
- **松键淡出时长**：不预设单一数值；钢琴、弦乐、单簧管分别做短/中/长 A/B 听测后冻结，并由 VoiceEngine 释放阶段执行。

## 独立固件与 USB 路线修订（2026-09-02，覆盖此前兼容边界）

1. 最终产物是独立 **A Band of One** 固件：保留课程母本已验证的 GPIO8 电源仲裁、按键扫描、编码器、LED、I2S/音频、分区与 BOOT 烧录方法；不启动、不交付 USB HID、BLE HID 或 EasyInput App 链路。
2. 正常运行时使用 ESP32-S3 内建 **USB-Serial/JTAG** 作为 Web Serial 唯一控制通道。网页只按 Espressif VID `0x303A` 缩小串口选择范围，随后必须通过 `ping → hello/state` 应用层握手确认固件；不把尚未真机冻结的 PID 当产品合同。
3. 原课程功能和 A Band of One 不在同一镜像并行。需要课程快捷键/EasyInput App 时重新烧录课程母本；切回本项目同样重新烧录相应镜像，始终不执行 `erase_flash`。
4. M3 的立即执行顺序为 **T03-U → T05-B → T03 → T04 → T05-C → T06**。先把可识别、可回滚的串口与乐谱事务打通，再让声音引擎和状态机接入，避免用未实现的板端协议误判网页完成。

## T03/P1 单簧管音频存储格式修订（2026-09-03，Task 0 已批准）

1. **保留内容不变**：仍为 23 枚源根音、21 个目标音 × 3 乐器 = 63 个可弹组合；单簧管仍使用 `D3/F3/A#3/D4/F4/A#4/D5/F5/A#5`，根音映射、实时变调、0.8 秒处理窗口、逐根循环终点和 20ms 离线交叉淡化均不变。
2. **存储格式冻结**：钢琴 7 枚和小提琴 7 枚继续使用 EIAD-v1；单簧管 9 枚改用连续 PCM16LE。这样解除高音区 EIAD 每 10ms 固定索引重置产生的电音，同时让单簧管 bank 只保留一种格式。
3. **预算合同**：目标音频 payload 为 `14 枚 EIAD + 9 枚 PCM16LE = 1,309,174B`（当前钢琴/小提琴 EIAD `617,974B` + 单簧管 PCM 数据 `691,200B`）；factory 仍为 3MiB，最终 app 必须 `< 2.4MiB`，运行时仍只申请 `1,402,514B` 固定 PSRAM PCM 仓。
4. **停止门与顺序**：Task 1 先以候选 PCM 与④ EIAD 做九根音单次/循环 A/B；人工未通过前不得改正式 `delivery/manifest.json`、不得改 CMake/固件、不得编译和烧录。Task 1 通过后已完成正式 PCM 资产清单与生成物替换；下一项 Task 3 才切换 CMake/固件加载分支；烧录仍需另行独立批准。
5. **禁止回退**：不恢复已被用户否决的自适应 EIAD 索引，不只替换高音五枚，不混入增益/滤波/新源文件调参；所有候选与正式产物继续保留来源、许可和 SHA-256。

## P2 HIL 缺陷修复门（2026-09-03）

旋钮按压的一次物理点击会产生 `Pressed` 与 `Released` 两个输入事件；应用层只允许 `Pressed` 触发一次 `on_encoder_press`，`Released` 只完成输入反馈，不得再次驱动 P2 状态机。先以源码契约测试复现并锁定该边界，再重建 ASCII 固件；未重新烧录前，P2 HIL 仍保持未通过。

## P2 模式控制语义修订（2026-09-04）

- 主阶段不再单列 Paused：产品语义冻结为“暂停 = Standby”。旋钮按压只负责 `Standby → Playing` 与 `Playing → Standby`；前者按当前模式开始或继续，后者立即停音并冻结跟谱游标、错误数与已用时间。
- 旋钮旋转只负责 Playing 阶段的音区调整；Standby 阶段不切换模式。
- 自由模式/跟谱模式由网页工作区选择，通过 USB-Serial/JTAG 的 `set_mode` 指令下发；板端只在 Standby 接受，演奏中回报拒绝。Standby 内若切换模式则结束并清零旧模式的本次进度；未切换模式再次按旋钮则继续原模式。
- 网页收到板端 `state.mode` 后再作为模式真相；切换失败不擅自更新模式。

## P3 Web／键盘协同与 `.abo.score.json` 合同修订（2026-09-05，Task 0、Task 1、Task 2 与 Task 4 GREEN 已完成）

本节把 P3 审核中已确认的交互与协议写回计划契约；后续实现必须以本节、`docs/技术方案.md` §4/§6 和 `docs/Web控制台功能框架.md` 为准。

1. **板端持续状态是真相**：`state` 可选增量字段为 `score_crc32`、`note_ticks`、`note_total_ticks`；其中 `score_crc32` 为当前活动谱 canonical CRC32，`note_ticks` 必须满足 `0 <= note_ticks <= note_total_ticks`。Waiting、Standby、Finished 的当前音进度为 0；网页只在收到合法状态后更新。
2. **实体输入单独回传**：板端对 S1--S9 发出不可合并的 `input` 事件，字段为 `seq:uint32`、`control:s1..s9`、`phase:pressed|released`。`state.keys` 继续承担持续按住/待按颜色；`input` 只承担旋钮瞬态反馈与曲终确认，不替代状态真相。
3. **旋钮职责不重叠**：实体 S9 按压是 `Standby → Playing`、`Playing → Standby`、`Finished → Standby`；网页旋钮只显示状态，不发送开始/暂停/继续，也不成为第二套演奏输入。网页只可在 Standby 发送 `set_mode(score|free)`；`set_volume(0..100)` 可在 Standby/Playing 平滑调节；Finished 发送 `transport(action=reset)` 关闭成绩并回 Standby。
4. **曲终优先级**：Finished 阶段任意实体 S1--S9 的 `Pressed` 只确认并回 Standby，输入被消费，不发声、不切换乐器、不重新开始；网页的完成卡片等收到 `state.phase=standby` 后再关闭。
5. **音量边界**：`set_volume` 在 Standby/Playing 只改 RAM 目标值，默认 70 与现有 0dB 基线相同；主任务写入 `InstrumentVoiceSession` 的单槽 atomic 目标邮箱，唯一音频消费者在渲染块边界更新 mixer，并在 480 帧（10ms）内平滑到目标。网页滑块只做预览，80ms 防抖后发送最后值，必须等待 ACK→state。
6. **乐谱范围**：P3 完整支持本机 UTF-8 `.abo.score.json`（`abo.score v1`）：选择/拖放、严格校验与错误定位、预览、显式 `score_begin/chunk/commit`、canonical CRC32、ACK/state 确认、红蓝引导、拍长游标、中央固定指针与活动谱带平滑移动、拔线清理与重连同谱恢复。MIDI、MusicXML、PDF/图片识谱、网页内 AI、API Key 与第三方上传留作后续。
7. **导入帮助**：导入工作区先显示“会话曲库”，再显示“如何用 AI 生成乐谱文件”。静态提示词只识别主旋律；钢琴大谱表只取高音谱表，忽略低音伴奏、和弦及伴奏升降号；仅输出 `schema、version、id、title、bpm、notes`，固定 `schema:"abo.score"`、`version:1`，`id` 使用 ASCII，BPM 为 60—180 整数，未标注时用 90，音符 `{n,b}` 的 `n=0..6`、`b` 为 0.25 倍数且四分音符为 1；删除休止空拍，无法辨认、和弦、主旋律升降音或节拍不确定时先列待确认项，确认后只输出 JSON。结果保存为 UTF-8 的 `曲名.abo.score.json` 并人工核对；网页不调用 AI 服务。
8. **谱面呈现**：内置《小星星》使用完整 42 音；任意合法导入谱共用同一渲染器。谱面区使用中央固定指针，只有当前活动的五线谱或简谱轨道按实际音符中心以 `translate3d` 平滑移动；隐藏记谱视图不得参与坐标计算。指针位置来自板端真实状态、当前游标与 `note_ticks/note_total_ticks`，板端每约 100ms 发布进度，网页只能在相邻真实快照之间做 CSS 视觉过渡，不得用浏览器计时器推进真实游标。Waiting、Playing 中等待、Standby（游标非零）、Finished 或断线时保持当前位置；权威 `standby + cursor=0`、成功装载新谱、记谱法切换和窗口尺寸变化走即时复位/重算路径，第二轮从第一音可见位置开始。

**执行门**：Task 0 合同同步、Task 1 RED→GREEN、Task 2 主音量实时路径 RED→GREEN、网页 Task 4 RED→GREEN、谱面固定中央指针 Task 0—4 与 T06 最终整机主流程已完成；本轮全部 16 个 console 测试文件通过，嵌入脚本解析通过，谱面及整机 COM5 人工验收通过。当前只剩 C7/C8 证据归档与最终交接；本轮不重新构建、不烧录。

## 音量真机分层诊断（2026-09-05，Task 0、Task 1 与 HIL 已完成）

零改动 HIL 已确认右侧音量数字始终为 70、无法显示 0。根因位于网页音量状态机：板端每 100ms 发布旧 `state.volume=70`，网页 80ms 防抖尚未把用户目标登记为 pending 时，状态处理会把 `volumeDesired` 和显示值都覆盖回 70，最终继续发送 70；现有测试只覆盖 `SerialManager`，没有执行 DOM 音量时序。Task 1 RED→GREEN 已完成：用户目标保护、在途最新值合并、ACK+匹配 state 确认均已实现；全部 16 个 console 测试和两个嵌入脚本语法检查通过。用户已确认 COM5 人工 HIL 通过：音量可调节且能反馈到键盘。本问题关闭，不改音源、不增加固件遥测、不构建、不烧录。详细步骤见 `docs/superpowers/plans/2026-09-05-volume-control-hil-diagnosis.md`。

## 实时进展 / 交接棒
→ 见 `flow/进展.md` 顶部（每棒收工在那追加一条：做了什么 / 为什么 / 产出路径 / 下一步）。
（plan.md 只管"计划=契约"；"现在到哪了"在进展日志，不在这儿覆盖。）
