# A Band of One · P1 原始采样清单

本目录只存放可追溯的原始素材和转换后的交付素材；不存放来源不明的音频。

## 已锁定来源

| 乐器 | 根音目标 | 来源 | 许可 | 原始下载物 | 状态 |
|---|---|---|---|---|---|
| 钢琴 | B2 / F#3 / C4 / F#4 / C5 / F#5 / C6 | FreePats Upright Piano KW | CC0 1.0 | `raw/UprightPianoKW-small-SFZ-20190703.7z` | 7 枚原始 WAV 已在 `raw/piano-upright-kw/samples/`，原件与交付物 SHA-256 见 `delivery/manifest.json`；7z SHA-256 `FFB547FCEB91EEB93D78CF8D220D1D5F726DCAC891AFC15E517805AC48ABB39D` |
| 弦乐（当前正式方案） | G3 / C4 / E4 / G4 / C5 / E5 / A5 | VSCO-2-CE（sgossner）Solo Violin `Arco Vib f` | CC0 1.0 | `raw/violin-vsco2/` 7 枚 | 已人工放置并完成网页试听准入；G3--B5 的最近根音变调不超过 ±2 半音，C3--F#3 是小提琴下限造成的例外；原件与交付物 SHA-256 见 `delivery/manifest.json` |
| 长笛（历史候选，已废弃） | C3 / C4 / C5 | VSCO-2-CE（sgossner）Flute susNV，力度层 v1 | CC0 1.0 | `raw/flute-vsco2/` 3 枚（同上） | 保留来源证据，不再进入当前三乐器产品 |

## 当前正式：单簧管

| 乐器 | 根音目标 | 来源 | 许可 | 当前本地文件 | 状态 |
|---|---|---|---|---|---|
| 单簧管 | D3 / F3 / A#3 / D4 / F4 / A#4 / D5 / F5 / A#5 | nbrosowsky/tonejs-instruments | CC BY 3.0（项目级接受，原始作者未核实；必须携带归属） | `raw/clarinet-tonejs/` 9 枚 WAV | 已完成完整 21 音网页听感与 EIAD 转换；归属见 `incoming/tonejs-clarinet-probe/ATTRIBUTION.md`，原件与交付物 SHA-256 见 `delivery/manifest.json` |

## SHA-256（原始 WAV，2026-09-01 核验）

- 当前分区弦乐：`susvib_C3_v1_1.wav` `A614EE7CA821B44660236E47B89620CF914EC6AD27A99BC0D9764F47DBE512E1`；`ViolaEns_susvib_C4_v1_1.wav` `144EC7666BE1B3B4DA50937DB4B1FC3675CA34BC2E871264707D83B8A0D526E6`；`LLVln_ArcoVib_C5_f.wav` `AF72F852EF2892233699851DACF5F8D97D446822D8EB4CE9E0EC27D5A721B118`。完整来源 URL 见 `raw/strings-vsco-zoned/SOURCE_MANIFEST.json`。
- `LLVln_ArcoVib_G3_f.wav` `80BAE3776B0CCD3D4774F47429E829C7764F5FE6061FF844A8AFFC6AC68B7899`
- `LLVln_ArcoVib_C4_f.wav` `4169605B51A72454F11BC2C34EFC66C1D9993FD2B7D5F097A025EF9D3A2DBDFA`
- `LLVln_ArcoVib_C5_f.wav` `AF72F852EF2892233699851DACF5F8D97D446822D8EB4CE9E0EC27D5A721B118`
- `LDFlute_susNV_C3_v1_1.wav` `4F221909317E257E4D0BC49F10479643C80CB213A0486455C1BDC1D6ED13A450`
- `LDFlute_susNV_C4_v1_1.wav` `105A6DBCED98DE7AE04A317BDD3BA1A5C6B90DC94034439B68CCE6635E2781DF`
- `LDFlute_susNV_C5_v1_1.wav` `46A335B804C9B2F081EFA8E6C695F155D05B3700C69BF73950CC9CB40ED53D41`

## 处理合同

1. 原始文件保持不改；每项记录来源链接、下载日期、许可、根音和 SHA-256。
2. 转换目标为单声道、48 kHz、16-bit PCM；钢琴截取自然衰减约 1.5–2 秒。
3. 弦乐和单簧管的交叉淡化循环段与短 release 仍须经过 VoiceEngine 宿主测试后才可进入完整固件分区。
4. IMA-ADPCM（EIAD）已生成 23 枚；总量以 `delivery/manifest.json` 的实测为准，不能以旧 450KiB 估算替代。

## P1 当前交付资产（2026-09-02）

`delivery/` 由 `tools/prepare_p1_assets.py` 从上述原始 WAV 可重复生成；转换器只使用 Python 标准库，绝不改写 `raw/`。

- 23 枚 EIAD v1：48kHz、单声道、PCM16 解码目标、-3dBFS 峰值归一化。
- 钢琴每枚 2.0 秒、小提琴每枚 1.5 秒、单簧管每枚 0.7 秒；清单记录初始循环区和短 release 的设计边界。
- 当前 EIAD 合计 **773,134B**，低于 780KiB 上限；每个交付文件、源文件和处理结果的 SHA-256 均在 `delivery/manifest.json`。
- 解码 PCM 实测：钢琴 **1,402,514B**、小提琴 **1,008,000B**、单簧管 **604,800B**；运行时只分配最大单乐器固定 PCM 仓，不同时常驻钢琴与小提琴。
- 宿主验证：`C:\esp\v5.5.5\esp-idf\python_env\idf5.5_env\Scripts\python.exe tests\test_p1_asset_manifest.py`。

## 来源

- FreePats Acoustic Grand Piano 页面：<https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html>
- 该小型 SFZ/WAV 音库（CC0 1.0）：<https://freepats.zenvoid.org/Piano/UprightPianoKW/UprightPianoKW-small-SFZ-20190703.7z>
