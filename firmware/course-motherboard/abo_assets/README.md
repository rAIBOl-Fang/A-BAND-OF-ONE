# A Band of One · 音频资产

本目录是 A Band of One 的音频来源、交付文件、归属和哈希清单。公开版本以 `delivery/manifest.json` 和 `delivery/ATTRIBUTION.md` 为准；不要根据历史试听页或旧的转换记录推断当前固件资产。

## 当前正式存储合同

| 乐器 | 根音数量 | 交付格式 | 交付内容 |
|---|---:|---|---|
| 钢琴 | 7 | EIAD-v1 | `B2 / F#3 / C4 / F#4 / C5 / F#5 / C6` |
| 弦乐 | 7 | EIAD-v1 | `G3 / C4 / E4 / G4 / C5 / E5 / A5`，同源 Solo Violin `Arco Vib f` |
| 单簧管 | 9 | PCM16LE-v1 | `D3 / F3 / A#3 / D4 / F4 / A#4 / D5 / F5 / A#5` |

当前正式交付资产合计 **14 枚 EIAD-v1 + 9 枚 PCM16LE-v1**，payload 为 **1,309,174 bytes**。所有正式资产目标为 48 kHz、单声道、16-bit；每个文件的格式、样本数、来源文件和 SHA-256 以 [`delivery/manifest.json`](delivery/manifest.json) 为准。

## 目录说明

```text
abo_assets/
├─ delivery/       固件正式使用的 23 枚交付资产、manifest 和归属文件
├─ incoming/       单簧管原始探针及其来源说明，用于来源核对和试听
├─ raw/            已纳入来源追溯的原始音频与原始下载物
├─ tests/          音频格式、清单、哈希和交付合同测试
└─ tools/          音频核对与资产处理工具；不依赖本机固定路径
```

公开版本不包含长笛候选、分区弦乐候选或历史 A/B 试听页。它们不属于当前三乐器正式资产合同；相关历史过程不应被当作当前固件输入。

## 来源与许可证

- 钢琴： [FreePats Upright Piano KW](https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html)，CC0 1.0。
- 弦乐： [VSCO-2-CE](https://github.com/sgossner/VSCO-2-CE) 的 Solo Violin `Arco Vib f`，CC0 1.0。
- 单簧管： [nbrosowsky/tonejs-instruments](https://github.com/nbrosowsky/tonejs-instruments)，按项目级 CC BY 3.0 声明记录；原始作者未核实，必须保留归属说明。

逐文件来源、许可、转换说明、源文件哈希和交付文件哈希见 [`delivery/ATTRIBUTION.md`](delivery/ATTRIBUTION.md) 与 [`delivery/manifest.json`](delivery/manifest.json)。新增或替换音频时，必须同步更新这两个文件。

## 可重复性与边界

1. `raw/` 与 `incoming/` 用于来源追溯和人工核对；固件构建直接使用 `delivery/`，不在构建时下载音频。
2. `delivery/` 是公开版本唯一的正式音频输入；不要重新引入未发布的历史候选目录。
3. 资产处理脚本只能写入明确的输出目录，不得改写 `raw/`；处理结果必须经过测试、哈希核对和归属复核后才能替换 `delivery/`。
4. `managed_components/` 是 ESP-IDF 依赖生成物，不属于音频资产，也不应提交；依赖由固件目录中的 `main/idf_component.yml` 与 `dependencies.lock` 重建。

## 相关入口

- 项目根 README：[`../../../README.md`](../../../README.md)
- 音频归属：[`delivery/ATTRIBUTION.md`](delivery/ATTRIBUTION.md)
- 正式清单：[`delivery/manifest.json`](delivery/manifest.json)
- 固件工程说明：[`../README.md`](../README.md)
