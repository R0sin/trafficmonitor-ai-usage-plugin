# Sub2API 用量支持

此 fork 新增四个 TrafficMonitor 额度条，沿用原版的进度条样式：

| 显示项 | 标签 | 内容 |
| --- | --- | --- |
| Codex API 5h | A5h | 5 小时额度已用百分比 |
| Codex API 1d | A1d | 1 天额度已用百分比 |
| Codex API 7d | A7d | 7 天额度已用百分比 |
| Codex API Key Quota | AKey | API key 总额度已用百分比 |

百分比为 `used / limit * 100`，进度条最多填满 100%。悬浮提示显示
已用／上限、额度所属范围，以及服务端提供的重置时间（UTC）。
不再显示钱包余额、请求数和 Token 数。
原有 Claude 和 Codex 百分比显示项的 ID 和顺序不变。

## 安装与使用

1. 安装 Node.js 22 或更高版本，并将 `node` 加入启动 TrafficMonitor 时的 PATH。
   已配置 Claude helper 的用户可复用同一运行时，无需 Python 或额外浏览器。
2. 编译后将整个 `build\x64\Release\plugins` 目录部署到 TrafficMonitor。
   必须包括 `ClaudeUsagePlugin\codex-api-helper.ps1` 和
   `ClaudeUsagePlugin\helper\codex-api-usage.mjs` 及其许可证文件，不能只复制 DLL。
   构建会自动安装依赖并打包，无需手动复制 `node_modules`。
3. 在 Cockpit 切换到 Sub2API 的 API key 账号。
4. 重启 TrafficMonitor，在任务栏显示项目中启用上面四个新项目。
   若安装过本 fork 的旧测试版，需要重新选择显示项，原来的余额／请求／Token 项已移除。

插件读取 `%USERPROFILE%\.codex`，也支持 `CODEX_HOME`。
helper 从 `config.toml` 当前 `model_provider` 对应的 `base_url` 获取地址，
优先读取该 provider 的 `experimental_bearer_token`，未配置时读取
`auth.json` 的 `OPENAI_API_KEY`，无需重复保存密钥。
是否可查询由当前 provider 地址和有效 API key 决定，不以 `auth_mode` 判断账号类型。
允许同时绑定 OAuth（例如用于生图）；查询用量仍使用 API key，不使用 OAuth token。
例如基础地址 `https://www.sub2api.com/v1` 对应 `GET https://www.sub2api.com/v1/usage`，
通过 `Authorization: Bearer <API key>` 认证。
基础地址只有域名时，首先请求 `/usage`，仅在 404 时尝试 `/v1/usage`。

可在源码目录手动刷新一次：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\codex-api-helper.ps1
```

## 数据与限制

- 以标准 [Sub2API Usage 实现](https://github.com/Wei-Shaw/sub2api/blob/main/backend/internal/handler/gateway_handler.go) 为依据。
- 周期额度读取 `rate_limits[]` 的 `5h`、`1d`、`7d` 项，使用 `limit` 和 `used`。
  `reset_at` 存在时显示重置时间；窗口以服务端为准，不假定为自然日或自然周。
- 订阅模式下，如果没有对应的 key 周期限额，日／周条使用
  `subscription.daily_limit_usd` / `daily_usage_usd` 及
  `weekly_limit_usd` / `weekly_usage_usd`。提示中明确标记 Subscription。
  `quota_limited` 模式不会混入订阅额度，也不猜测订阅重置时间。
- 总额度条仅使用 `quota.limit` / `quota.used`，不会将余额或 `remaining` 视作总额度。
- 没有正数上限、缺少已用值或声明无限额时显示 `--`，不会当成 0%。
  纯钱包模式可能没有任何可显示的额度条。
- 每约 60 秒在独立进程中请求一次，网络操作不会阻塞 TrafficMonitor 刷新线程。
  单次 helper 最长运行 25 秒；快照超过 180 秒后失效。
- DLL 每秒核对配置指纹，检测变更后清空旧指标，并等待配置稳定两秒再查询。
  查询期间检测到账号变化时丢弃结果；指纹不匹配的快照不会显示。
- 快照位于 `%LOCALAPPDATA%\trafficmonitor-claude-usage-plugin\codex-api-usage.ini`。
  使用 UTF-16 INI 供 Windows 原生 API 读取，仅包含指标、状态、时间和配置的 SHA-256 指纹。
  不包含 API key、原始响应或服务商地址。
- 支持 HTTPS，拒绝跳转，避免将密钥转发到另一个地址。
- 第一版支持文件中的 API key 和当前顶层 provider 配置。
  支持 OAuth 绑定时仅在 provider 的 `experimental_bearer_token` 中保存 API key。
  暂不支持系统凭据库、provider 的环境变量密钥覆盖、
  profile 选择和 Cockpit 本地网关转发配置。此类配置需要后续单独适配。
- 原有 `X5h` / `X7d` 仍读取会话日志，与 Sub2API 服务商设置的额度不同，
  使用 API key 时请启用新项目。

## 验证

直接构建解决方案即可：MSBuild 自动通过 `npm ci` 安装锁定的构建依赖，
再用 esbuild 将 helper 和 TOML 解析库 `smol-toml` 打包成一个 `.mjs`。
esbuild 仅用于构建，发布包无需 `node_modules`，使用者无需运行 npm。

```powershell
MSBuild.exe .\ClaudeUsagePlugin.sln /t:Build /p:Configuration=Release /p:Platform=x64
node --test .\tests\codex-api-usage.test.mjs
.\build\x64\Release\tests\CodexUsagePluginTests.exe api-snapshot
.\build\x64\Release\tests\CodexUsagePluginTests.exe .\build\x64\Release\plugins\ClaudeUsagePlugin.dll api-bars
```

完整 DLL 仍需要 Visual Studio 的 MSVC v143 MFC 组件。
