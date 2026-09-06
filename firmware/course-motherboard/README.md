# A Band of One · ESP32-S3 固件

本目录是 **A Band of One** 的独立 ESP32-S3 固件工程。它保留 EasyInput V2.0 已验证的硬件平台层，在此基础上实现三种乐器、自由演奏、跟谱判定、音频资产加载和 Web Serial 控制协议。

> 这不是 EasyInput Maker 上游项目的 README，也不是要求用户去 clone 另一个仓库的说明。A Band of One 的上游硬件与平台关系、许可证和派生边界见仓库根目录的 [`THIRD_PARTY_NOTICES.md`](../../THIRD_PARTY_NOTICES.md)；老师项目与开发环境来源见根目录 [`README.md`](../../README.md)。

## 当前产品边界

- **目标硬件**：EasyInput V2.0 / ESP32-S3。
- **实体输入**：S1–S7 演奏 7 个音级，S8 循环切换钢琴、弦乐和单簧管；旋钮旋转调整音区，旋钮按压开始、暂停或继续。
- **演奏模式**：自由演奏不判定；跟谱演奏由板端严格判定，弹错不推进时间轴。
- **网页通道**：使用 ESP32-S3 内建 USB-Serial/JTAG，网页通过 `ping → hello/state` 握手后进行乐谱、模式、音量和状态交互。
- **正式音频**：14 枚 EIAD-v1（钢琴 7 + 弦乐 7）与 9 枚 PCM16LE-v1（单簧管 9），详细清单以 [`abo_assets/delivery/manifest.json`](abo_assets/delivery/manifest.json) 为准。
- **明确不交付**：A Band of One 产品模式不启用、不依赖 USB HID、BLE HID、EasyInput App、麦克风网络上行或课程母本的应用链路。源码中保留的上游组件不等于本产品运行边界。

## 硬件安全边界

| 引脚 / 部件 | A Band of One 约束 |
|---|---|
| GPIO8 | LED、麦克风、扬声器共用的高有效电源域；不得当作普通灯光开关，也不得绕开现有电源仲裁。 |
| GPIO12 | 5 颗 WS2812 数据线。 |
| GPIO19/20 | ESP32-S3 原生 USB D-/D+，用于 USB-Serial/JTAG。 |
| GPIO0 | BOOT 引脚，不是第 9 个业务按键。 |
| Flash | 切换课程母本与 A Band of One 镜像时重新烧录对应镜像；任何流程都禁止 `erase_flash`。 |

修改 GPIO、BOOT、USB、I2S、音频电源或分区前，先阅读 [`docs/hardware/easyinput-v2-safety.md`](docs/hardware/easyinput-v2-safety.md)。

## 在其他电脑上构建

### 前置条件

- Windows 或其他 ESP-IDF 支持的平台；
- ESP-IDF **5.5.5** 及匹配的 Python 环境；
- 编译目录必须是纯 ASCII 路径。不要直接在包含中文、`#` 或空格的工作区路径中编译；
- 不需要提交 `managed_components/`。它是依赖管理器生成物，可由 `main/idf_component.yml` 与 `dependencies.lock` 重建。

### Windows 示例

先将仓库 clone 到纯 ASCII 路径，例如 `C:\esp-work\A-BAND-OF-ONE`，再执行：

```powershell
Set-Location C:\esp-work\A-BAND-OF-ONE
git checkout v0.1.0-mvp

# 按本机环境地图装配已经安装好的 ESP-IDF 5.5.5 环境
. C:\esp-work\tools\idf-env.ps1

Set-Location firmware\course-motherboard
idf.py --version
idf.py set-target esp32s3
idf.py build
```

看到 `Project build complete` 才算完成一次构建。不要重装已有 ESP-IDF，不要新建虚拟环境，不要运行 `eim install` 或 `eim fix`。

默认流程只构建，不自动烧录。烧录前必须单独确认板型、USB 端口、BOOT 状态和要写入的镜像；未经确认不要执行 `idf.py flash`，任何情况下都不执行 `erase_flash`。烧录与恢复说明见 [`docs/getting-started/flash-and-recovery.md`](docs/getting-started/flash-and-recovery.md)。

## 测试与证据

网页契约测试不需要第三方 npm 依赖：

```powershell
Set-Location C:\esp-work\A-BAND-OF-ONE
node console/index.test.mjs
```

公开版本保留的 8 个 console 测试、内嵌脚本解析、ESP-IDF 构建和 EasyInput V2.0 真机验收属于不同证据，不能相互替代。历史试听探针不随当前公开快照发布。当前已验收结论与回退边界见 [`docs/最终交接.md`](../../docs/最终交接.md)。

音频资产在构建前应检查：

- [`abo_assets/README.md`](abo_assets/README.md)
- [`abo_assets/delivery/ATTRIBUTION.md`](abo_assets/delivery/ATTRIBUTION.md)
- [`abo_assets/delivery/manifest.json`](abo_assets/delivery/manifest.json)

## 目录地图

```text
course-motherboard/
├─ main/                       固件入口、平台层和 IDF 组件声明
├─ abo_host/                   USB-Serial/JTAG 主机协议
├─ abo_p0/                     启动自检
├─ abo_p1/                     音频声部与按键触发
├─ abo_p2/                     演奏时钟、跟谱状态机和节奏灯
├─ components/                 键盘、音频、电源和平台组件
├─ features/speaker_assets/    正式音频资产加载与运行时
├─ host_test/                  纯 C++ 宿主契约测试
├─ abo_assets/                 原始音频、交付资产、归属和清单
├─ dependencies.lock           IDF 依赖锁定文件
└─ partitions.csv              固件分区表
```

构建产物 `build/`、`managed_components/`、`sdkconfig` 和本机缓存均由仓库 `.gitignore` 排除。提交源码或资产时，不要把本机路径、凭据、编译产物或未归属的音频带入公开仓库。
