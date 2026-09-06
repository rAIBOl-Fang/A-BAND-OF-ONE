# 项目宪章 (charter)

> 立项填。所有 Agent 开工要读的第一份。

- **项目名**：A Band of One · 一个人的乐队（EasyInput 第三课作业）
- **目标**（做成什么样算成功，一句话）：在 EasyInput V2.0 键盘真机上跑通"三乐器跟谱演奏器"——8 键弹 21 音、网页控制台红蓝引导与成绩统计，15 秒主流程可现场演示。
- **范围**：
  - 做：需求定义（已完成）、独立 A Band of One 固件（声音引擎 / 双模式状态机 / USB Serial/JTAG 协议）、单文件 HTML 控制台、`abo.score` 本地导入与下发、烧录与真机验收、flow 交接记录。
  - 不做：EasyInput USB HID / BLE / EasyInput App 产品链路、挑战模式（时间轴不停 + Miss）、Electron 打包、Looper 叠录、网页内乐谱编辑与 MIDI/PDF/图片识谱（演进方向见 PRD §08）。
- **约束**（时间 / 资源 / 必须遵守的）：
  - 板级硬约束以 PRD §06 为准（ESP-IDF 5.5.5、GPIO8 共享电源域、禁 erase_flash、BOOT/USB 引脚保留等）。
  - 固件编译必须在纯 ASCII 路径下进行（项目目录名含空格，编译前复制到如 C:\esp-work）。
  - 方法论参照王照涵 easyinput-beatbox（本地副本 C:\esp-work\beatbox-ref\easyinput-beatbox-main），复刻的是 P0-P3 推进顺序，不是抄代码。
  - 最终固件正常运行时只提供 ESP32-S3 内建 USB-Serial/JTAG 控制链路，不维持课程母本的 HID/BLE/App 行为；恢复课程功能需重新烧录课程母本，切换过程同样禁止 `erase_flash`。
- **成功标准**（尽量可衡量）：
  1. 15 秒主流程真机可玩：选曲 → 网页红光 → 按键 → 板子出声 → 蓝光持续拍长。
  2. 钢琴 / 弦乐 / 单簧管三种音色 0.5 秒内可被人耳区分。
  3. 曲终"弹错 N 次、用时 M 秒"统计正确。
  4. 全程 flow 六要素交接记录完整（对应课程评分 C7 / C8）。
- **角色**：拍板 = 用户 · 主控 = TRAE / Claude Code · 评审 = 换模型互审。
