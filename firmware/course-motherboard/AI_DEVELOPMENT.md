# AI 开发入口

本文件给进入仓库的 AI Agent 一条最短、公开且可验证的工作路径。完整协作合同以 `AGENTS.md` 为准。

## 开始前

1. 阅读 `AGENTS.md`、当前任务涉及的代码和对应 `docs/` 文档。
2. 涉及引脚、BOOT、GPIO8、USB、音频或睡眠时，必须先读 `docs/hardware/easyinput-v2-safety.md`。
3. 不猜测板级事实，不把构建成功或静态扫描写成实板成功。
4. 不把密钥、Wi-Fi 凭据、设备标识、个人路径或原始私有日志写入源码、测试、文档和提交信息。

## 默认开发回路

1. 把需求缩小为一个可观察结果。
2. 优先把可验证的纯逻辑放在 `components/keyboard/`，平台硬件适配留在 `main/platform/`。
3. 先加失败测试，再修改实现。
4. 运行全部宿主测试，再运行 ESP-IDF 5.5.5 默认构建。
5. 报告真实证据和未验证项，不自动烧录硬件。

```bash
cmake -S host_test -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
idf.py build
```

如果当前 shell 找不到 `cmake` 或 `idf.py`，应先激活 ESP-IDF 5.5.5 环境，而不是把某台电脑上的 SDK 绝对路径写入项目。

## 硬件写入门禁

烧录会覆盖开发板上的固件。只有在使用者明确要求、重新识别当前连接设备并确认目标为 ESP32-S3 后才能执行。进入和退出下载模式必须遵守 `docs/getting-started/flash-and-recovery.md`，不得自行编造通用 BOOT/RESET 手势。

面向学习者的完整 AI 工作方式和灯光练习在 `docs/teaching/ai-vibe-coding.md`。
