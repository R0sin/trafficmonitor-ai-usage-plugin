# TrafficMonitor AI Usage Limits v0.3.13-sub2api.3

补齐 Sub2API 订阅周额度重置时间，并精简悬浮提示。

## 更新内容

- 订阅周额度按服务端 `weekly_window_start` 加 7 天显示重置时间。
- API key 周期额度继续使用服务端 `reset_at`；倒计时和本地时间格式与 Codex 一致。
- 悬浮提示中的百分比四舍五入为整数，去除 `used`、`Subscription`、来源标签和更新时间。
- 金额单独换行，格式为 `115.13/200.00 USD`，去除分号及斜杠两侧空格。
- 任务栏百分比精度及缓存过期校验保持不变。

## 下载与升级

- 64 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.3_x64.zip`
- 32 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.3_x86.zip`
- `SHA256SUMS` 提供 ZIP 的 SHA-256 校验值。

退出 TrafficMonitor，备份旧插件，将对应 ZIP 解压到 TrafficMonitor 安装目录并覆盖后重新启动。
必须同时更新 DLL 和 helper。需要 Node.js 22+，无需运行 npm。

## 验证与限制

- 已通过 x64／x86 Release 构建、打包 helper 回归测试、快照、DLL 绘制和 Codex 窗口分类测试。
- 已在本地真实 Sub2API 账号上验证订阅周重置时间，并在运行中的 x64 TrafficMonitor 加载插件。
- 服务端未提供有效重置时间或订阅周窗口起点时，不显示重置时间。
- 保留预发布状态；非官方 OpenAI、Anthropic 或 TrafficMonitor 插件。
