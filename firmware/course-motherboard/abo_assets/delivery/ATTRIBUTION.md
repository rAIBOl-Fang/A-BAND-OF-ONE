# A Band of One · 声音资产归属与许可

本文件随 `delivery/` 中的 23 枚 EIAD 转换资产分发。逐文件的原始文件 SHA-256、EIAD SHA-256、根音、循环点、样本数和字节数，以同目录的 [`manifest.json`](manifest.json) 为准；本文件不替代清单中的哈希校验。

## 钢琴

- 来源项目：FreePats Upright Piano KW
- 许可：CC0 1.0
- 项目页面：<https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html>
- 下载包：<https://freepats.zenvoid.org/Piano/UprightPianoKW/UprightPianoKW-small-SFZ-20190703.7z>
- 本项目使用的正式根音：B2 / F#3 / C4 / F#4 / C5 / F#5 / C6

## 弦乐

- 来源项目：VSCO-2-CE Solo Violin（sgossner）`Arco Vib f`
- 许可：CC0 1.0
- 项目页面：<https://github.com/sgossner/VSCO-2-CE>
- 本项目使用的正式根音：G3 / C4 / E4 / G4 / C5 / E5 / A5

## 单簧管

- 来源项目：`nbrosowsky/tonejs-instruments`
- 项目页面：<https://github.com/nbrosowsky/tonejs-instruments>
- 上游项目级样本许可声明：CC BY 3.0
- 本项目使用的正式根音：D3 / F3 / A#3 / D4 / F4 / A#4 / D5 / F5 / A#5
- 许可风险提示：当前上游 `sample-source-info.txt` 未为 `clarinet/` 提供逐文件原始录音者或原始库信息；本项目按用户确认接受项目级 CC BY 3.0 声明，但必须持续标注“原始作者未核实”，不得将其归为 Iowa、VSCO 或其他未经上游直接证明的录音库。

## 分发要求

1. 保留本文件、上述来源链接与 CC0 / CC BY 3.0 许可说明。
2. 修改、替换或重新生成任一 EIAD 后，必须同步更新 [`manifest.json`](manifest.json) 中对应的 `source_sha256`、`eiad_sha256`、字节数及处理元数据，并重新运行资产契约测试。
3. 本文件只说明归属，不改变音频内容、映射、循环点或运行时缓存策略。
