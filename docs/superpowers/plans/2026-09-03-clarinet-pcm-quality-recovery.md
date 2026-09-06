# 单簧管高音电音恢复 Implementation Plan

> 状态：已完成。历史实施计划，仅供追溯，不再作为当前待办；单簧管音质继续作为后续维护候选。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 保留已经通过的 9 根音映射与相位匹配循环，把 F4 以上由 EIAD 帧量化产生的电音移除，并完成单簧管 21 音短音/长音真机验收。

**Architecture:** 钢琴和小提琴继续使用现有 EIAD；单簧管 9 根音统一改为 48kHz、mono、PCM16LE，并沿用已试听较好的 `0.8s + 每根音独立循环终点 + 20ms 离线交叉淡化`。`InstrumentVoiceSession` 在换仓安全态按资产存储类型选择 EIAD 解码或 PCM16LE 拷贝，运行时仍只使用现有 1,402,514B 固定 PSRAM 仓，不新增第二个 bank。

**Tech Stack:** ESP-IDF 5.5.5、ESP32-S3、C++17、48kHz PCM16LE、EIAD/IMA-ADPCM、PSRAM、I2S DMA、Python 标准库、Node.js 单文件网页测试、CMake/CTest。

**Spec:** `docs/技术方案.md` §3.3--§3.7、`flow/踩坑记录.md` 顶部单簧管条目、`console/diagnose-clarinet-hil.html` 四路径听测结果；路径④唯一来源为 `firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-preview/manifest.json`，不得改用 `clarinet-tonejs-high-ab-preview`。

## Investigation Findings

- 用户确认路径④比②③明显改善，说明“逐根音相位终点 + 20ms 离线交叉淡化”有效，应保留。
- 诊断页路径④对 9 枚根音原速播放，没有执行 21 音实时变调；因此 F4、D5、F5、A#5 的电音不是最近根音映射或错误 playback rate 造成。
- 原 WAV 与④ EIAD 在稳定段的基频一致：F4 约 `349.9Hz`、D5 约 `588.7Hz`、F5 约 `701.1Hz`、A#5 约 `934.7Hz`，相对各自原音只差约 `+3.5/+4.0/+6.6/+4.4 cents`。用户听到的“赫兹过高”是高频量化噪声的听感，不是基频被升高。
- EIAD-v1 每 `480` 个采样重启一帧，即每 `10ms` 重启一次；现有编码器把每帧 IMA 步长索引固定为 `0`。每帧前 32 个采样只占 `6.7%`，却贡献各根音约 `91.8%--97.4%` 的总重建误差，形成 100Hz 周期及其谐波的电音/抖动。
- ④的整体/循环 SNR 从 F4 往上下降：F4 `33.60/33.71dB`，A#4 `28.57/29.55dB`，D5 `26.81/26.08dB`，F5 `27.73/25.05dB`，A#5 `18.14/20.93dB`；A#5 帧边界 SNR 只有 `6.55dB`。该趋势与人工听感一致。
- 先前“每帧自适应索引”虽把离线 SNR 提高，但用户试听时 9 音均不可辨，已经回退并列入禁用死路；本计划不恢复该算法。
- 当前 app 为 `1,099,376B`。正式单簧管 EIAD 为 `155,160B`；9 枚 `0.8s` PCM16LE 为 `691,200B`。全部替换后的估算 app 为 `1,635,416B`（约 1.56MiB），低于 2.4MiB 门槛约 `881,166B`；3MiB factory 预计仍余 `1,510,312B`。单簧管 PCM bank 为 `691,200B`，仍小于由钢琴决定的现有固定仓 `1,402,514B`。

## Global Constraints

- [ ] 本文件是待用户确认的实施计划，不授权自动执行；开始 Task 0 前必须取得用户明确批准，烧录前还要取得一次独立批准。
- [ ] 不更换 Tonejs 单簧管来源，不下载新素材，不删减 9 枚根音，不改变 21 个目标音及最近根音映射。
- [ ] 不修改钢琴、小提琴资产、播放参数或已通过的固定 PSRAM 仓架构。
- [ ] 不恢复已被人工听感否决的 EIAD 自适应索引，不在同一轮混入增益、滤波、变调或 release 调整。
- [ ] E: 为源码真相源；ESP-IDF 编译前复制到全新纯 ASCII 目录 `C:\esp-work\abo-p1-clarinet-pcm-fw`，目标已存在时停止并报告，不覆盖旧构建。
- [ ] 编译前只执行 `. C:\esp-work\tools\idf-env.ps1`；不重装 ESP-IDF、不新建 venv、不运行 `eim install/fix`。
- [ ] 永不执行 `erase_flash`，不写 `sound_a`/`sound_b`，不启动 HID、BLE 或 EasyInput App。
- [ ] 最终硬门槛：app `< 2.4MiB`、PSRAM 固定仓 `1,402,514B` 不增长、单簧管切仓 `< 500ms`、按键到声音 `< 50ms`。

## File Map

- `firmware/course-motherboard/abo_assets/tools/prepare_clarinet_pcm_preview.py`：只生成独立 PCM 听测候选，不碰正式 delivery manifest。
- `firmware/course-motherboard/abo_assets/tests/test_clarinet_pcm_preview.py`：固定九根音、处理参数、循环点、样本数、哈希与 PCM 字节数。
- `firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/`：独立候选包；包含 9 枚 WAV、manifest 与归属文件。
- `console/listen-clarinet-pcm-ab.html` / `.test.mjs`：同根音比较“处理后 PCM 单次/循环”和“④ EIAD 单次/循环”。
- `firmware/course-motherboard/abo_assets/tools/prepare_p1_assets.py`：通过听测后才把正式单簧管输出改为 `.pcm16le`。
- `firmware/course-motherboard/abo_assets/tests/test_p1_asset_manifest.py`：允许钢琴/小提琴 EIAD + 单簧管 PCM16LE，固定新预算。
- `firmware/course-motherboard/features/speaker_assets/include/speaker_assets/abo_p1_sound_bank.h`：为根音资产增加明确的存储类型和通用 payload 字段。
- `firmware/course-motherboard/features/speaker_assets/abo_p1_sound_bank.cpp`：嵌入 14 枚 EIAD 和 9 枚 PCM16LE，固定九枚单簧管循环点。
- `firmware/course-motherboard/features/speaker_assets/CMakeLists.txt`：只把正式 9 枚 `.pcm16le` 加入产品镜像，不再嵌入正式单簧管 `.eiad`。
- `firmware/course-motherboard/main/platform/instrument_voice_session.cpp`：在静音换仓边界分派 EIAD 解码或 PCM16LE 拷贝。
- `firmware/course-motherboard/host_test/abo_p1_asset_decode_tests.cpp`：分别验证 EIAD 完整解码与 PCM16LE 完整拷贝。
- `firmware/course-motherboard/host_test/abo_p1_sound_bank_contract_tests.cpp`：固定存储类型、字节数、样本数和循环点。
- `firmware/course-motherboard/host_test/abo_instrument_voice_contract_tests.cpp`：固定混合存储加载分支和单仓边界。
- `firmware/course-motherboard/host_test/abo_voice_renderer_tests.cpp`：用真实的 38,400-sample 根音长度验证短按、长按与新循环边界。
- `flow/plan.md`、`flow/decisions.md`、`docs/技术方案.md`、`flow/tasks/T03-T06-Luna执行.md`：用户批准后同步契约，消除“23 枚全部 EIAD”的过期描述。

---

### Task 0: 用户批准后修订计划契约

**Files:**
- Modify: `flow/plan.md`
- Modify: `flow/decisions.md`
- Modify: `docs/技术方案.md`
- Modify: `flow/tasks/T03-T06-Luna执行.md`

**Interfaces:**
- Consumes: 本计划的调查证据与用户批准。
- Produces: “钢琴/小提琴 EIAD + 单簧管 PCM16LE”的唯一正式路线。

- [x] **Step 1: 更新冻结结论。**

  将“23 枚全部 EIAD”改为“14 枚 EIAD + 9 枚 PCM16LE”；明确 23 枚根音、63 个目标音、9 枚单簧管根音和最近根音映射均不变。把旧的 `780KiB EIAD 总包`门槛替换为 `1,309,174B 音频 payload 估算 + app < 2.4MiB` 双门槛。

- [x] **Step 2: 追加决策而不改写历史。**

  在 `flow/decisions.md` 末尾追加新决策：保留④的相位循环/交叉淡化，单簧管统一 PCM16LE；否决固定索引 EIAD、自适应索引和只替换高音五枚的混合根音格式。理由是全 9 枚 PCM 只比“高音五枚 PCM”多约 228,400B，却让同一单簧管 bank 只有一种格式和一致听感。

- [x] **Step 3: 检查契约一致性。**

  Run:

  ```powershell
  rg -n "23 枚全部|23 枚源根音 IMA|780KiB|听感优化已延后" flow/plan.md flow/tasks/T03-T06-Luna执行.md docs/技术方案.md
  ```

  Expected: 当前有效段落不再把单簧管限定为 EIAD；历史决策段落可以保留，但必须有新决策覆盖指针。

### Task 1: 建立“不经 EIAD”的 PCM 听感停止门

**Files:**
- Create: `firmware/course-motherboard/abo_assets/tools/prepare_clarinet_pcm_preview.py`
- Create: `firmware/course-motherboard/abo_assets/tests/test_clarinet_pcm_preview.py`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet-tonejs-pcm-preview/manifest.json`
- Create: `console/listen-clarinet-pcm-ab.html`
- Create: `console/listen-clarinet-pcm-ab.test.mjs`

**Interfaces:**
- Consumes: 9 枚 `raw/clarinet-tonejs/*.wav`、④清单中的逐根音循环终点。
- Produces: 9 枚处理后 PCM WAV 与 `AboClarinetPcmAbCore` 浏览器测试接口。

- [x] **Step 1: 写资产 RED 测试。**

  测试断言根音严格为 `D3/F3/A#3/D4/F4/A#4/D5/F5/A#5`；每枚为 48kHz、mono、PCM16、38,400 samples；raw loop start 为 `19,200`，crossfade 为 `960`，正式 loop start 为 `20,160`，loop end 依次为 `27,984/36,912/27,984/32,400/29,472/31,056/29,376/30,432/27,552`。每枚 PCM data 必须为 `76,800B`，总 PCM 为 `691,200B`，source/output SHA-256 必须与 manifest 一致。

- [x] **Step 2: 运行测试确认 RED。**

  Run:

  ```powershell
  & 'C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe' firmware/course-motherboard/abo_assets/tests/test_clarinet_pcm_preview.py
  ```

  Expected: FAIL，原因是 preview 生成器或 manifest 尚不存在。

- [x] **Step 3: 实现最小 PCM preview 生成器。**

  复用 `prepare_p1_assets.read_wav/normalize` 和已经通过的 `bake_loop_crossfade`。输出 WAV 只包含④同样的 0.8s、-3dB 峰值归一化、20ms 交叉淡化；不经过 EIAD，不裁起音，不加滤波或增益补丁。循环参数只读取 `delivery/clarinet-tonejs-preview/manifest.json`；禁止读取 `clarinet-tonejs-high-ab-preview`。复制现有单簧管 `ATTRIBUTION.md` 到候选目录。

- [x] **Step 4: 写网页 RED 测试并实现 A/B 页面。**

  `AboClarinetPcmAbCore` 必须暴露 9 根音、PCM/④ EIAD URL 和四种播放计划：PCM 单次、PCM 循环、④ EIAD 单次、④ EIAD 循环。点击新按钮先停止旧声音；循环点只从各自 manifest 读取。

- [x] **Step 5: 运行 GREEN 与网页回归。**

  Run:

  ```powershell
  & 'C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe' firmware/course-motherboard/abo_assets/tests/test_clarinet_pcm_preview.py
  node console/listen-clarinet-pcm-ab.test.mjs
  node console/diagnose-clarinet-hil.test.mjs
  node console/listen-clarinet-eiad.test.mjs
  ```

  Expected: 全部 PASS；现有④资产哈希不变。

- [x] **Step 6: 人工听感停止门。**

  用户对 9 根音逐一比较，F4/A#4/D5/F5/A#5 各听单次起音和至少 4 秒循环。只有 PCM 单次无电音、PCM 循环无明显间隔且比④稳定，才进入 Task 2。若 PCM 单次在前 0.38 秒已经失真，只检查源 WAV、PCM 写出/读取和 normalize，不调整循环点；若前 0.38 秒正常、到交叉淡化尾段或循环后才异常，只检查对应根音的 loop/crossfade，不改 codec、增益或源文件。两种失败都必须停止，不进入固件。

### Task 2: 将正式单簧管资产改为 PCM16LE

**Files:**
- Modify: `firmware/course-motherboard/abo_assets/tools/prepare_p1_assets.py`
- Modify: `firmware/course-motherboard/abo_assets/tests/test_p1_asset_manifest.py`
- Modify: `firmware/course-motherboard/abo_assets/delivery/manifest.json`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_d3.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_f3.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_as3.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_d4.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_f4.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_as4.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_d5.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_f5.pcm16le`
- Create: `firmware/course-motherboard/abo_assets/delivery/clarinet_as5.pcm16le`

**Interfaces:**
- Consumes: Task 1 已人工通过的 PCM samples 和循环参数。
- Produces: manifest 中 14 个 `EIAD-v1` 与 9 个 `PCM16LE-v1` payload。

- [x] **Step 1: 先把新合同写成 RED。**

  `test_p1_asset_manifest.py` 必须断言 23 个 ID 不变；钢琴/小提琴仍有 `eiad_sha256`；单簧管有 `pcm_sha256`、`encoded_bytes=76800`、`total_samples=38400` 和 Task 1 的逐根音循环点。总 payload 固定为 `1,309,174B`，解码后钢琴/小提琴/单簧管分别为 `1,402,514/1,008,000/691,200B`，最大单 bank 仍为 `1,402,514B`。

- [x] **Step 2: 运行 RED。**

  Run:

  ```powershell
  & 'C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe' firmware/course-motherboard/abo_assets/tests/test_p1_asset_manifest.py
  ```

  Expected: FAIL，旧 manifest 的 9 枚单簧管仍为 33,600-sample EIAD。

- [x] **Step 3: 实现正式 PCM16LE 输出。**

  生成器对钢琴/小提琴保持逐字节输出不变；对 9 枚单簧管执行 Task 1 相同处理，并用 little-endian signed 16-bit 连续写出 `.pcm16le`。不删除旧 `.eiad` 文件；正式 manifest 不再引用它们，CMake/固件嵌入列表留到 Task 3 统一切换，避免在加载分支未实现时提前破坏构建合同。

- [x] **Step 4: 生成并验证 GREEN。**

  Run:

  ```powershell
  & 'C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe' firmware/course-motherboard/abo_assets/tools/prepare_p1_assets.py
  & 'C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe' firmware/course-motherboard/abo_assets/tests/test_p1_asset_manifest.py
  ```

  Expected: PASS，钢琴/小提琴 14 个 EIAD SHA-256 与修改前一致。

### Task 3: 给固定 PSRAM 仓增加 PCM16LE 加载分支

**Files:**
- Modify: `firmware/course-motherboard/features/speaker_assets/include/speaker_assets/abo_p1_sound_bank.h`
- Modify: `firmware/course-motherboard/features/speaker_assets/abo_p1_sound_bank.cpp`
- Modify: `firmware/course-motherboard/features/speaker_assets/CMakeLists.txt`
- Modify: `firmware/course-motherboard/main/platform/instrument_voice_session.cpp`
- Modify: `firmware/course-motherboard/host_test/abo_p1_asset_decode_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/abo_p1_sound_bank_contract_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/abo_instrument_voice_contract_tests.cpp`
- Modify: `firmware/course-motherboard/host_test/abo_voice_renderer_tests.cpp`

**Interfaces:**
- Consumes: `AboP1RootStorage::{EiadV1,Pcm16Le}`、`payload/payload_bytes`、Task 2 manifest。
- Produces: 同一 `VoiceEngine::RootSample` 数组；渲染器不感知 Flash 存储格式。

- [x] **Step 1: 写三个宿主 RED。**

  资产解码测试断言 9 枚 PCM payload 恰为 76,800B 并可还原 38,400 个样本；bank 测试断言钢琴/小提琴 storage 为 `EiadV1`、单簧管为 `Pcm16Le`；session 合同测试断言存在两个显式加载分支，且 `kMaxPcmSamples` 仍为 `701257U`。renderer 测试创建 38,400-sample 根音并使用 F4 的 `20160--29472` 循环点，分别验证短按进入 release、长按跨越至少两次循环且输出样本数连续、loop end 回绕后没有零样本间隙。

- [x] **Step 2: 运行目标测试确认 RED。**

  使用既有宿主测试构建目录 `firmware/course-motherboard/host_test/build-abo`（CMake/MinGW 通过绝对路径装配，不改系统环境）运行四个目标；旧实现因 `AboP1RootStorage`、`payload` 字段和 PCM 分支不存在而失败，renderer 新用例先因测试实现问题修正后再进入 RED。

- [x] **Step 3: 实现通用根音资产描述。**

  将根音描述字段统一为 `storage/payload/payload_bytes/decoded_samples/loop_start_sample/loop_end_sample`。`abo_p1_piano_c4_sound()` 继续把钢琴 C4 的 EIAD payload 适配给已验证的启动音接口，不改变启动音路径。

- [x] **Step 4: 实现 PCM16LE 安全拷贝。**

  `EiadV1` 继续使用 `SoundAssetStreamDecoder`；`Pcm16Le` 先验证 `payload_bytes == decoded_samples * sizeof(int16_t)`，再在 bank 安全态用 `memcpy` 拷入 PSRAM。两条分支都必须验证循环范围和累计样本总数；任何失败沿用现有 rollback 与 `bank_load_failed` 路径。

- [x] **Step 5: 更新嵌入列表与固定元数据。**

  CMake 保留 14 个 `.eiad`，改嵌入 9 个 `.pcm16le`。单簧管 bank 总样本改为 `345600U`，每根 `38400U`，loop start 均为 `20160U`，loop end 使用 Task 1 的九个值。

- [x] **Step 6: 运行目标 GREEN。**

  Expected: 四个目标测试 PASS；`VoiceEngine`、`VoiceRenderer`、`MonoVoiceMixer` 不需要源码修改。

### Task 4: 全回归、预算和 ASCII 构建停止门

**Files:**
- Build only: `C:\esp-work\abo-p1-clarinet-pcm-fw\`
- Update after evidence: `flow/进展.md`
- Update on failure: `flow/踩坑记录.md`

**Interfaces:**
- Consumes: Task 0--3 的 E: 真相源。
- Produces: 未烧录的候选 bin、map、大小、哈希和测试证据。

- [x] **Step 1: 复制到全新 ASCII 目录。**

  先 `Test-Path 'C:\esp-work\abo-p1-clarinet-pcm-fw'`；结果为 True 时停止。结果为 False 时用 `robocopy /E` 从 `firmware/course-motherboard` 复制，不使用 `/MIR`、`/PURGE` 或删除参数。

- [x] **Step 2: 运行资产、网页与全部宿主测试。**

  在 ASCII 副本执行 Python 资产测试；配置并构建 `build-host`；运行全部宿主测试。若本机 CTest 再出现已记录的 `0xc0000135` 启动器问题，按现有方法逐个执行测试二进制并记录真实通过数，不能把启动器失败记成测试通过。

- [x] **Step 3: 装配既有 ESP-IDF 并构建。**

  Run:

  ```powershell
  . C:\esp-work\tools\idf-env.ps1
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" --version
  & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" -B build -DABO_PRODUCT=ON build
  ```

  Expected: ESP-IDF v5.5.5；构建成功；app `< 2.4MiB`；factory 余量大于 0；map 中 14 个 EIAD 与 9 个 PCM16LE payload 各出现一次。

- [x] **Step 4: 记录但不烧录。**

  实际证据：资产 `7/7`、网页 `16/16`、宿主 CTest `70/70`；ESP-IDF `v5.5.5-dirty` 构建退出码 `0`；app bin `1,635,856B`、factory 余量 `0x1709f0`（48%）；map 唯一嵌入资产 `14` 个 EIAD + `9` 个 PCM16LE；固定 PSRAM 仓为 `701257` samples / `1,402,514B`；bin SHA-256 为 `F393B3D84D3EA39DE3A31D59C1A65FBAFBBA0E903DE693971D07EE382C2A520A`。Task 5 烧录仍需独立批准。

  记录 app 精确字节数、factory 余量、bin SHA-256、map 中资产计数、PSRAM 固定仓常量和完整测试数，然后停止并单独申请烧录批准。

### Task 5: 用户批准后的单簧管真机验收

**Files:**
- Flash source: `C:\esp-work\abo-p1-clarinet-pcm-fw\build\easy_input_keyboard.bin`
- Update after evidence: `flow/进展.md`
- Update on failure: `flow/踩坑记录.md`

**Interfaces:**
- Consumes: Task 4 的 SHA-256 已确认候选镜像和用户独立烧录批准。
- Produces: 单簧管 21 音短音/长音、切仓、连接和资源门槛证据。

- [x] **Step 1: 识别下载模式设备。**

  用户短按一次 BOOT；用 `Get-PnpDevice -PresentOnly` 确认 Espressif VID `303A`、实际 PID 和 COM。目标不唯一或没有 COM 时停止，不换线、不猜端口。

- [x] **Step 2: 标准 flash，不擦除。**

  实际证据：`Get-PnpDevice -PresentOnly` 确认 `USB\VID_303A&PID_1001 / COM5`；使用 Task 4 候选镜像执行 `idf.py -p COM5 -B build flash`，bootloader、app、partition-table 均返回 `Hash of data verified`，`FLASH_EXIT=0`；未执行 `erase_flash`，未写入 `sound_a/sound_b`。

  使用确认的 COM 执行 `idf.py -p COMx flash`。日志必须出现各分区 `Hash of data verified`；命令中不得出现 `erase_flash`。

- [ ] **Step 3: 正常关机再开机。**

  重开时不按 BOOT；验收五灯、钢琴 C4 启动音、`ping → hello/state` 和 S1--S8 输入回传均正常。

- [ ] **Step 4: 验收单簧管 21 个短音。**

  S8 切到单簧管；低/中/高三个音区各短按 S1--S7。21 个目标音必须都有声、音高顺序正确，重点记录 F4/G4/A4/B4、C5/D5/E5/F5/G5/A5/B5 是否仍有电音或不稳定。

- [ ] **Step 5: 验收 9 根音附近的长音。**

  每个音区长按 S1/S4/S7 至少 4 秒，并额外长按最接近 F4、A#4、D5、F5、A#5 的目标键至少 4 秒。要求起音清楚、持续段无明显间隔、无 10ms 电音感、松键正常淡出。

- [ ] **Step 6: 复验换仓和资源指标。**

  从钢琴开始按 S8 三次，记录钢琴→小提琴→单簧管→钢琴的 `bank_swap`；每次 `result=0`、`bank_load_ms < 500`，`arena_bytes=1402514`，free/largest PSRAM 切换前后无持续下降。

- [ ] **Step 7: P1 停止门。**

  单簧管 21 短音与长音通过、钢琴/小提琴无回归、按键延迟 `<50ms`、切仓 `<500ms`、app/PSRAM 门槛均通过后，才能完成原 P1 Task 6 Step 5/6 并进入 P2。任一听感失败时停止，不叠加第二种修复，回到 Task 1 的对应 PCM 单次/循环边界定位。

## Final Self-Review

- [ ] 确认 23 个根音 ID 和 63 个目标组合没有减少。
- [ ] 确认钢琴/小提琴 EIAD SHA-256 未变，单簧管原 WAV SHA-256 未变。
- [ ] 确认正式 CMake 不嵌入旧单簧管 EIAD，也不嵌入任何 preview 文件。
- [ ] 确认 `VoiceRenderer`、I2S、GPIO8、USB-Serial/JTAG、乐谱协议均未因存储格式调整而修改。
- [ ] 确认没有执行 `erase_flash`、没有写 `sound_a`/`sound_b`、没有覆盖旧 ASCII 构建。
- [ ] 由非实施模型复审代码、资产哈希、预算和 HIL 证据后，再宣称单簧管问题解决。

## Independent Plan Review

- 2026-09-03 使用非产出模型完成只读审查，结论为“无阻断项”；预算算术、固定单仓和 PCM 仅移除量化路径的边界均成立。
- 已采纳审查建议：锁定④的唯一 manifest；拆分单次前 0.38 秒与循环/交叉淡化的失败归因；加入 38,400-sample 真长度 renderer 测试。
