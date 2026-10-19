# TrafficMonitor AI Usage Limits v0.3.13-sub2api.4

统一 Sub2API 任务栏与悬浮提示的百分比精度。

## 更新内容

- 悬浮提示复用任务栏百分比文本，最多保留一位小数，整数省略 `.0`：例如 `12.5%`、`13%`。
- 修复上一版提示显示整数、任务栏显示小数的不一致。
- 保留重置倒计时、本地时间及换行显示的紧凑额度金额。

## 下载与升级

- 64 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.4_x64.zip`
- 32 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.4_x86.zip`
- `SHA256SUMS` 提供 ZIP 的 SHA-256 校验值。

退出 TrafficMonitor，备份旧插件，将对应 ZIP 解压到 TrafficMonitor 安装目录并覆盖后重新启动。
发布包包含 DLL、helper 和许可证文件；需要 Node.js 22+，无需运行 npm。

## 验证与限制

- 已通过 x64／x86 Release 构建、打包 helper 回归测试、快照、DLL 绘制和 Codex 窗口分类测试。
- 已在本机 x64 TrafficMonitor 加载验证；x86 未在真实应用中手动验证。
- 保留预发布状态；非官方 OpenAI、Anthropic 或 TrafficMonitor 插件。
