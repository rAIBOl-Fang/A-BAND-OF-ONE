# P3' Web／键盘实时反馈缺口审核与待执行计划

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办。

> 状态：**Task 0 合同同步、Task 1 RED→GREEN、Task 2 RED→GREEN 与 Task 4 RED→GREEN 已完成（2026-09-05）；等待 Task 5 网页人工预验收**。
> 本文件冻结问题证据、协议、实施顺序和停止门；Task 0 只改计划/方案/决策文档；Task 1 GREEN 已实现协议/板端状态边界，Task 2 GREEN 已实现音量实时路径，但仍不构建 ESP 固件、不烧录。

## 1. 本轮结论

当前 P2 核心链路已经通过用户真机验证：自由模式开始/暂停全停音、暂停即 Standby、Standby 网页切跟谱、装载《小星星》均可工作。新报告的五项现象属于 P3 控制台尚未接完整，而不是按键、GPIO8、I2S、PSRAM 或既有 P2 状态机整体失效：

1. 五线谱游标写死在页面 `left:25%`，网页没有消费板端 `phase/subphase/cursor`，板端也没有下发当前音符内部的拍长进度。
2. 曲终使用浏览器原生 `alert()`，无法继承页面视觉，也没有“关闭弹层并让板端回 Standby”的事务。
3. 两个音量滑块只更新浏览器本地数字；板端没有解析 `set_volume`，P1 mixer 也没有主增益入口。
4. 实体旋钮 S9 的 Pressed/Released 只进入板端状态机，没有发给 HostLink，网页只能看到最终 phase，无法呈现按压瞬态。
5. 旧内置《小星星》只有 14 个音符；长谱的 DOM 宽度和滚动没有按拍数计算。本轮合同已将目标定为完整 42 音与按拍数横向滚动。

导入谱并不局限于《小星星》：当前板端会根据已提交乐谱的每个 `n` 更新红色待按键和蓝色持续键。缺失的是网页游标渲染和长谱布局，而不是板端只能识别固定旋律。

## 2. 方案比较

### A. 板端时间真相 + 完整双向协议（推荐）

- 板端下发音符内拍长进度和实体输入事件；网页只插值显示，不自行计拍或判定。
- 音量经事务下发，在 P1 mixer 输出端平滑生效，再由板端 state 确认。
- 曲终弹层由板端 Finished/Standby 状态驱动，实体键和电脑键都能安全返回 Standby。
- 优点：状态一致、断线可恢复、后续导入任意合法谱可直接复用。
- 代价：需要网页与固件各一次实现，最终必须重新构建、另行批准烧录。

### B. 只修网页外观

- 用浏览器定时器移动游标、替换 `alert()`、本地模拟音量和旋钮反馈。
- 优点：快、无需烧录。
- 缺点：与板端暂停/错音卡住/拍长不一致，音量不会影响键盘，实体 S9 仍不可观测；断线重连后容易显示假状态。
- 结论：不能满足本项目“板端为真相”，不采用。

### C. 引入完整乐谱排版库并做纵向分页

- 可获得更专业的五线谱、分行和打印体验。
- 代价：显著增加单文件控制台体积与依赖，且演奏时跨行跳转不如连续轨道清晰。
- 结论：留作后续“谱面概览/打印”能力，不阻塞 P3。

## 3. 推荐交互合同

### 3.1 旋钮 S9

- 产品语义保持不变：实体旋钮按压负责开始／暂停／继续；网页上的旋钮只是状态显示，不成为第二套演奏输入。
- Standby：旋钮旁显示“按压开始”。
- Playing：显示“按压暂停”。
- Finished：显示“按任意键返回待机”。
- 收到 S9 `pressed` 后，跟谱和自由两个工作区里的旋钮都立即变黑，中心文字变为“按压”；收到 `released` 后恢复。
- 为避免真实点击过短而肉眼看不到，页面只在收到真实 `pressed` 后保证最短 120ms 视觉脉冲；不得用网页点击伪造实体输入。

### 3.2 曲终弹层

- 用页面内自定义 modal/card 替代 `alert()`，沿用当前黑、白、橙视觉变量。
- 展示曲名、错误次数、用时和“按任意键返回待机”。
- Finished 阶段的任意实体 S1--S9 Pressed 只作为“确认成绩并回 Standby”，本次输入被消费：S1--S7 不发声，S8 不切乐器，S9 不立即重新开始。
- 页面在收到板端 `state.phase="standby"` 后关闭弹层，不因单个瞬态事件而乐观关闭。
- 鼠标点击按钮或电脑键盘任意非修饰键时，页面发送 `transport(action="reset")`；收到 matching ACK 且随后 state=Standby 才关闭。断线时只能关闭本地展示，并明确标记设备状态未知。

### 3.3 谱面与游标

- 选择**横向连续谱带**，不在演奏过程中上下换行。
- 每拍占固定逻辑宽度，音符宽度按 `b` 成比例；二拍音符获得一拍音符两倍水平空间。
- 游标位置由“之前音符累计拍数 + 当前音符已走拍数”计算。
- 在可视区前 70% 内游标正常向右移动；超过阈值后容器平滑横向滚动，使当前音保持在约 60% 位置，并保留后续音符预览。
- Waiting、Standby 和断线时游标冻结；Holding 时只跟随板端进度；错音不得推进。
- 板端现有 state 发布周期为 100ms，网页仅用约 100--120ms CSS transition 连接相邻真实快照，不建立独立计拍器。
- 128 音符上限内不需要虚拟列表；未来概览/打印可另做纵向分行视图。

### 3.4 导入谱动态行为

- 当前 P3 格式范围已经用户确认：**只完整支持本机 `.abo.score.json`**；MIDI、MusicXML、PDF/图片识谱均不进入本期。
- “完整支持”包含：文件选择/拖放、结构与范围校验、错误定位、曲库预览、显式装载、begin/chunk/commit 原子事务、CRC/ACK/state 确认、红蓝键指导、拍长游标、长谱滚动、拔线清理与重连后的同谱恢复。
- 本期不在网页内调用 AI、不要求 API Key、不上传用户乐谱到第三方，也不产生模型或接口持续费用。
- S1--S7 的文字仍固定为 do--si；发生变化的是红色目标键、蓝色持续键和谱面游标。
- 每次成功 `score_commit` 后，网页保存该会话的规范 JSON；只有板端 `score_id + score_crc32` 与本地谱一致时才渲染并推进，防止同 ID 不同内容错配。
- 页面刷新或换浏览器后，如果板端已有谱但本地找不到相同 CRC，不得套用《小星星》显示；应提示重新导入对应 `.abo.score.json`。
- v1 继续限制：1--128 个单声部自然唱名音符，`n=0..6`，`b=0.25..8` 且为 0.25 倍数，BPM 60--180；本期不增加休止、升降号、和弦或逐音八度。

#### 导入区 AI 转换帮助

在导入控件正下方放置静态浅灰说明卡，不连接任何 AI 服务。文案冻结为：

> **如何用 AI 生成乐谱文件**  
> 将清晰、正向的单声部五线谱图片上传给支持识图的 AI，并发送：  
> “请将五线谱转换为 `abo.score v1` JSON。输出 `schema、version、id、title、bpm、notes`；音符使用 `{n,b}`，其中 do–si 对应 0–6，`b` 为拍数。遇到和弦、升降音、休止符或无法辨认的内容，请列出并让我确认，不要猜测。只输出 JSON。”  
> 将结果保存为 UTF-8 编码的 `曲名.abo.score.json` 后导入。AI 可能识别错误，请先核对音符、节拍和 BPM。

说明卡只负责教用户在外部 AI 中操作；不得出现“自动转换”按钮、API Key 输入框、第三方上传或暗示识别结果一定正确的文案。

### 3.5 完整《小星星》

- 内置谱从旧的 14 音扩展为常用完整 42 音版本：A-A-B-B-C-C-B / F-F-E-E-D-D-C / G-G-F-F-E-E-D ×2 / A-A-B-B-C-C-B / F-F-E-E-D-D-C；每句末音为 2 拍，其余为 1 拍。
- 实际 JSON 仍使用 `n/b` 数字，不把上面的文字说明直接写入协议。
- 更新后必须通过 42 音数量、总拍数、首尾序列、CRC 和横向滚动测试。

## 4. 候选协议增量

协议仍为 v1，采用向后兼容的可选字段；旧网页可忽略新字段，新网页遇到缺失字段时降级为“逐音跳转游标”，不得自行计拍。

### 4.1 板端 → 网页：state 增量

```json
{
  "t":"state",
  "v":1,
  "mode":"score",
  "phase":"playing",
  "subphase":"holding",
  "score_id":"twinkle",
  "score_crc32":"89abcdef",
  "cursor":12,
  "note_ticks":32,
  "note_total_ticks":96,
  "volume":70
}
```

- `score_crc32`：8 位小写十六进制，必须是板端当前活动谱的 canonical CRC32。
- `note_ticks`：当前音已完成 tick；Waiting/Standby 为 0。
- `note_total_ticks`：当前 cursor 对应音符总 tick；无活动音、无谱或 Finished 时为 0。
- 必须满足 `0 <= note_ticks <= note_total_ticks`；越界消息整体拒绝，保留网页最后一份有效状态。
- `cursor == note_count` 只允许 Finished；网页在不知道本地 note_count 时不猜测。

### 4.2 板端 → 网页：不可合并的实体输入事件

```json
{"t":"input","v":1,"seq":104,"control":"s9","phase":"pressed"}
{"t":"input","v":1,"seq":105,"control":"s9","phase":"released"}
```

- `control` 为 `s1`--`s9`；`phase` 为 `pressed|released`。
- `seq` 为板端单调递增 `uint32`，允许自然回绕；网页用它去重和发现明显丢包。
- `input` 进入关键事件 FIFO，不能像 state 一样合并；state 仍是持续状态最终真相。
- S1--S8 的 `state.keys` 保留，用于持有/待按颜色；`input` 只负责瞬态视觉和 Finished 确认，不替代 `state.keys`。

### 4.3 网页 → 板端：音量

```json
{"t":"set_volume","v":1,"tx":"12ab34ce","value":70}
```

- `value` 必须为整数 0--100；非法值返回 error，不静默钳制。
- 网页拖动时只预览数字，80ms 防抖后只发送最后值；同一时刻只允许一个 mutating request 在途。
- 成功顺序为 matching ACK → state；两个滑块只在 state 确认后成为一致的设备值。
- RAM 生效，不写 NVS；重启恢复默认 70。

### 4.4 网页 → 板端：仅用于曲终返回

```json
{"t":"transport","v":1,"tx":"12ab34cf","action":"reset"}
```

- 本批次只接 `reset`，且仅 Finished 接受；网页不远程 start/pause/resume，保持实体旋钮的产品语义。
- 其他阶段返回 `invalid_phase`；重复 tx 不重复改变状态。

## 5. 音量实现设计

- 主增益放在 `MonoVoiceMixer` 混音/交叉淡化之后、PCM16 饱和之前，保证三种乐器和新旧声部使用同一设备音量。
- 默认值 70 必须等于当前固件 0dB，避免修复音量功能时改变已验收的钢琴/小提琴听感。
- 建议映射：0=静音；1--69 从约 -40dB 平滑上升到 0dB；70=0dB；71--100 最多提升到 +3dB。具体使用 101 项整数增益表，由测试锁定 0/30/70/100 锚点。
- HostLink/主任务不得直接调用 mixer。`InstrumentVoiceSession` 增加单槽 atomic 目标音量邮箱（含递增 generation）；主任务只发布目标，唯一音频消费者在 `render_frame()` 的音频块边界读取新 generation，再调用 mixer 更新目标增益。该邮箱不随 bank swap 的 note command queue 清空，切仓后仍保持设备音量。
- mixer 在 480 帧（10ms@48kHz）内线性追到目标，避免拉动滑块时产生 click/zipper noise。
- 音频实时路径禁止 mutex、malloc、日志和浮点幂运算；乘法使用足够宽的整数中间值并最终 PCM16 饱和。
- `state.volume` 表示已接受的目标值；可听增益在最多 10ms 内完成平滑。

## 6. 待执行任务（必须逐门批准）

### Task 0：冻结合同，不写功能（已完成，2026-09-05）

修改：`flow/plan.md`、`docs/技术方案.md`、`docs/Web控制台功能框架.md`、`flow/decisions.md`、既有 P3 implementation plan。

- [x] 把本文件中用户确认的字段、状态迁移、横向滚动、完整《小星星》和音量曲线写入真相源。
- [x] 明确纠正旧 P3 草案中“网页旋钮可 start/pause/resume”：本期网页只可在 Standby 发 `set_mode`、在 Standby/Playing 发 `set_volume`、曲终发 `transport reset`。
- [x] 将 `.abo.score.json` 完整支持范围和导入区外部 AI 提示词写入真相源；网页不内置 AI/API/上传。
- **停止门**：Task 1 RED 已由用户的“下一步”批准并完成；Task 1 GREEN 已由后续“下一步”确认执行并完成。Task 2 仍需用户单独批准；在批准前不得实现音量路径、网页功能、构建 ESP 固件或烧录。

### Task 1：先写 RED——协议与板端状态

测试文件：Host wire/control、P2 controller、PerformanceRuntime contract、TX queue。

- state 新字段格式、范围和 512/768B 行长预算。
- input S1--S9 press/release、seq、FIFO 顺序、短写/背压、断线清理。
- Finished 任意 Pressed → Standby，动作被消费；Released、重复帧和其他阶段不误触发。
- `transport reset` 仅 Finished 成功，ACK 后 state；非法 phase/tx/action 不改状态。
- 已执行：新增 `host_test/abo_p3_protocol_red_tests.cpp` 并注册 `abo_p3_protocol_red_tests`；先编译并按预期失败，锁定上述缺口。
- GREEN 已执行：补齐 `state` 进度字段与 canonical CRC32、S1--S9 `input` 事件和关键 TX FIFO/短写保留、Finished 按键消费、`transport reset` 的 Finished 边界与 ACK→state 顺序；`PerformanceRuntime` 接入板端进度和复位回调。
- 验证：聚焦 P3/P2/协议测试通过；纯 ASCII 副本全量 CTest 77/77 通过。未构建 ESP 固件，未烧录。
- 停止门：Task 1 GREEN 完成；下一门为 Task 2 主音量 RED，必须另行批准。

### Task 2：先写 RED——主音量实时路径

> 状态：RED 已按预期失败，GREEN 已完成；聚焦混音器/音量合同 2/2 PASS，纯 ASCII 宿主全量 CTest 78/78 PASS。未构建 ESP 固件，未烧录。

测试文件：`abo_mono_voice_mixer_tests`、voice session/runtime contract。

- 0 静音、70 位完全等于旧输出、100 不溢出、正负峰值对称饱和。
- 10ms ramp 单调、跨 buffer 连续、交叉淡化期间改变音量不爆点。
- 快速 10→90→20 只追最新目标，无锁、无分配合同。
- 停止门：P1 所有既有声音测试必须保留，不能用更新 golden 掩盖响度回归。

### Task 3：实现板端最小增量

> 状态说明：本节中属于协议、输入事件、Finished 边界和板端状态的最小增量已在 Task 1 GREEN 完成；音量 setter、混音器渐变和 session 消费边界已在 Task 2 GREEN 完成；网页接入仍归 Task 4。

修改范围：`abo_host/wire_messages.*`、`abo_host/host_link.*`、P2 controller/types、`PerformanceRuntime`、`abo_app_main.cpp`、P1 mixer/session 及相应 CMake/测试登记。

- 输入扫描器保持原有去抖；只在应用出口复制事件给 HostLink，不建立第二扫描器。
- `input` 与 result/judge/ack/error 使用关键 FIFO；频繁 state 继续只保留最新快照。
- state 的 note 进度直接来自 `PerformanceState::score_ticks` 和活动谱当前 ticks。
- 音量 setter 不触碰资产、bank、变调或 I2S 生命周期；串口/主任务只能写 session 的 atomic 邮箱，只有音频消费者能修改 mixer/renderer 状态。
- 停止门：本节已纳入 Task 1 GREEN 的聚焦测试与纯 ASCII 全量 CTest（77/77）验证；仍不构建、不烧录。

### Task 4：先写 RED 再实现网页

修改范围：`console/index.test.mjs`、`console/index.html`；不重排 UI agent 的整体布局。

> RED 已执行：`node console/index.test.mjs` 按预期以退出码 1 失败，报告 28 项网页契约缺口；GREEN 已完成，全部 16 个 `console/*.test.mjs` 通过，两个嵌入脚本经 `vm.Script` 解析通过。

- fake Serial 覆盖 input、state 新字段、乱序/缺字段/越界、断线、tx ACK+state。
- 给导入区新增稳定 DOM 标识 `id="ai-score-help"` 的静态帮助卡；测试锁定标题、`只输出 JSON`、UTF-8 文件名与人工核对警示，并断言该区域不发网络请求、不收集 API Key。
- 两个 S9 视图同时变黑并显示“按压”，释放后恢复，最短脉冲只由真实 input 触发。
- 自定义 modal 覆盖实体键、电脑键、鼠标、重复键、断线和 reset 失败。
- 42 音《小星星》与任意合法导入谱使用同一渲染器；谱宽按拍数计算。
- 游标等待冻结、Holding 前进、错音不动、暂停不动、超过阈值自动横向滚动。
- 两个音量滑块 80ms 防抖、只有最后值发送、板端 state 回滚/确认、断线清空 pending。
- 停止门：Node 全量通过后先在 fake Serial／模拟输入页面人工验收，不烧录。

### Task 5：网页人工预验收

由用户在不烧录的情况下确认：

1. S9 按下视觉为黑色＋“按压”，提示文案符合 Standby/Playing/Finished。
2. 完整 42 音谱可横向浏览；模拟板端进度时游标与自动滚动清晰。
3. 完成卡片风格可接受，电脑键/鼠标交互清晰。
4. 两处音量值同步，不出现旧值跳回或重复请求。
5. 导入区下方的 AI 提示词说明精简可读，明确“外部 AI 转换、保存为 `.abo.score.json`、导入前人工核对”，且页面不要求登录或 API Key。

未通过只迭代网页/协议模拟；不得提前进入真机循环。

### Task 6：全量验证、异模型审稿与 ASCII 构建

- 运行 console 全量 Node 测试、资产测试、纯 ASCII 全量 CTest。
- 使用既有 `C:\esp-work\tools\idf-env.ps1` 和 ESP-IDF 5.5.5；复制到明确的纯 ASCII 目录后构建。
- 检查 app <2.4MiB、factory=3MiB、PSRAM 峰值仍 <2MiB、无 HID/BLE/EasyInput 产品符号。
- 由与实现模型不同的模型只读审查协议类型、事件队列、实时音频路径和失败回滚。
- 输出 bin/elf/map 的大小与 SHA-256；**等待用户单独批准烧录**。

### Task 7：最终真机 HIL

用户批准后才按既有 BOOT→端口核验→标准 flash 流程执行，永不运行 `erase_flash`。正常重启后逐项验收：

1. P0/P1 回归：五灯、启动 C4、63 音、S8 切乐器、长短音不回归。
2. S9：Standby 按压时网页旋钮变黑/显示按压，设备进入 Playing；再次按压进入 Standby，页面同步。
3. 谱面：完整《小星星》及一份不同导入谱都能按谱改变红蓝键；Waiting/错音/暂停冻结，正确 Holding 随节拍移动。
4. 长谱：第一屏结束后横向自动滚动，不跳页、不遮住后续目标。
5. 曲终：成绩准确；S1、S8、S9 各抽测一次均只关闭并回 Standby，不误发声/切乐器/重启；电脑键和鼠标也能通过 reset 同步关闭。
6. 音量：0/30/70/100 四点听测并观察 state；0 静音、70 与现有基线一致、快速拖动无爆点，两处滑块一致。
7. 故障：演奏中拔线板端继续；重连按 `score_id+score_crc32` 恢复，未知谱不显示错误旋律；pending 全清理。

## 7. 主要风险与回退

- **事件淹没**：input 不得复用 latest-state 槽；队满必须有计数/错误证据，不能静默把 S9 press 丢掉。
- **同 ID 错谱**：仅比较 `score_id` 不够，必须带 canonical CRC32。
- **音频爆点**：默认 70 保持 bit-equivalent；所有增益变化必须 ramp，实时线程禁锁/禁分配。
- **网页成为第二时钟**：禁止 `setInterval` 推进真实游标；CSS 只在两份板端快照之间做视觉过渡。
- **完成键副作用**：Finished 的确认输入必须在 S8 切仓和 NoteOn 之前被消费。
- **协议行变长**：实现前用格式化测试证明最大 state 不超 TX 行合同，不能现场扩大无界 buffer。
- **回退**：保留当前已验收 Standby 镜像及其 SHA-256；候选失败时重新烧录该镜像，同样不做全盘擦除。

## 8. 审核状态

- 当前 Codex 已完成源码与合同证据审核。
- 首次 Luna 独立审稿因账号用量上限未能运行；随后由非当前 GPT-5.4 模型完成只读复核。复核确认总体方向，并要求先冻结 `note_ticks` 真值、Finished 输入优先级、S9 独立 input 事件、音量消费者线程边界和横向按拍布局；这些要求已合并进本草案。
- Task 0 已由用户的“下一步”确认执行；合同已同步至 `flow/plan.md`、`docs/技术方案.md`、`docs/Web控制台功能框架.md`、`flow/decisions.md` 和既有 P3 implementation plan。Task 1 RED→GREEN、Task 2 RED→GREEN 已完成并通过 78/78 纯 ASCII 宿主回归；网页 Task 4 RED 已按预期报告 28 项缺口，随后 GREEN 已完成，全部 16 个 console 测试文件通过。当前进入 Task 5 网页人工预验收；Task 6 构建与 Task 7 烧录仍未执行。
