# Codex 本地配置

先复制 `config.example.json` 为私有的 `config.json`，填写绝对工作区和 MCP 私有目录。不要提交真实配置。

在 Codex 的 MCP 配置中保留现有条目并添加一个 STDIO server，命令指向 `notegen-mcp.exe`，参数为 `--stdio --config C:\...\config.json`。若手工编辑，先创建带时间戳备份并在保存后验证 TOML/JSON 格式。不要设置 URL 或端口。

先运行 `notegen-mcp.exe --config C:\...\config.json --check`，再重启 Codex，查看工具列表是否含 `notegen_get_status`。卸载时只删除该 MCP 条目和程序目录，保留工作区及回收站直到人工确认。

故障排查：协议日志不能出现在 stdout；启动错误在 stderr。`CONFIG_INVALID` 通常表示路径不存在或真实配置仍使用模板路径。

