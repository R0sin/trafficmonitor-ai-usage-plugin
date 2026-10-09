# TrafficMonitor AI Usage Limits v0.3.13-sub2api.2

修复 Sub2API 账号绑定 OAuth（例如用于生图）后无法查询额度的问题。

## 更新内容

- 不再根据 `auth_mode` 排除 API key 查询，允许 OAuth 与自定义 provider API key 共存。
- 优先读取当前 provider 的 `experimental_bearer_token`；未配置时读取 `auth.json` 中的 `OPENAI_API_KEY`。
- 修复 API key 仅保存在 provider 配置时出现 `No file-based Codex API key` 的问题。
- 用量查询不使用 OAuth token；继续拒绝 HTTP、重定向及不支持的环境变量密钥覆盖和 profile 配置。

## 下载与升级

- 64 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.2_x64.zip`
- 32 位 TrafficMonitor：`TrafficMonitorAIUsageLimits_v0.3.13-sub2api.2_x86.zip`
- `SHA256SUMS` 提供 ZIP 的 SHA-256 校验值。

退出 TrafficMonitor，备份旧插件，将对应 ZIP 解压到 TrafficMonitor 安装目录（含 `TrafficMonitor.exe` 的目录），覆盖插件后重新启动。
发布包包含 DLL、helper 和许可证文件，不能只替换 DLL。需要 Node.js 22+，无需运行 npm。

## 验证与限制

- 已通过 x64／x86 Release 构建、打包 helper 回归测试、快照、DLL 绘制和 Codex 窗口分类测试。
- 已在本地真实 Sub2API 配置上验证 OAuth 与 provider bearer token 共存时查询成功。
- 保留 Sub2API 预发布状态；未设置额度上限的项目可能显示 `--`。
- 系统凭据库、环境变量密钥覆盖、profile 和 Cockpit 本地网关配置暂不支持。
- 非官方 OpenAI、Anthropic 或 TrafficMonitor 插件。
