# TrafficMonitor AI Usage Limits v0.3.13-sub2api.1

基于上游 0.3.13 的 fork 预发布版，新增 Sub2API 额度监控。

## 更新内容

- 新增 A5h、A1d、A7d 和 AKey 四个已用百分比进度条。
- 支持 API key 周期及总额度；订阅模式支持日／周额度。
- 跟随 Codex 当前账号配置，切号时清空旧数据，快照超过 180 秒失效。
- 复用 Node.js 22+，无需 Python；发布包已包含打包后的 helper，无需运行 npm。
- 保留原有 Claude 和 Codex 会话额度显示项。

## 下载与升级

- 64 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.1_x64.zip`
- 32 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.1_x86.zip`
- `SHA256SUMS` 提供上述 ZIP 的 SHA-256 校验值。

先退出 TrafficMonitor，并通过原插件的 `claude-web-helper.ps1 stop` 停止后台
Claude watcher。备份旧插件后，将 ZIP 解压到 TrafficMonitor 安装目录，覆盖
整个对应插件目录，重启后启用 `Codex API 5h`、`Codex API 1d`、
`Codex API 7d` 和 `Codex API Key Quota`。

## 限制与验证

- 需要 Node.js 22+；本版支持 Sub2API `/usage`，不是所有中转的通用用量接口。
- 未配置上限的额度条显示 `--`；纯钱包账号可能没有可显示的额度条。
- 从 `CODEX_HOME`（默认 `%USERPROFILE%\.codex`）读取文件配置，不依赖 Cockpit 窗口。
- 系统凭据库、环境变量密钥覆盖及 Cockpit 本地网关配置暂不支持。
- 已通过 x64／x86 构建、自动测试和 DLL 绘制检查；仍需在实际服务商和
  TrafficMonitor 环境中验证。此版本标记为预发布版。
- 非官方 OpenAI、Anthropic 或 TrafficMonitor 插件。
