# ChatGPT 本地 MCP 配置

使用 ChatGPT 桌面客户端支持的本地 STDIO MCP 配置入口，程序为 `notegen-mcp.exe`，参数为 `--stdio --config C:\...\config.json`。不同桌面版本的配置 UI/文件位置可能变化，应先定位当前实际配置、创建时间戳备份、保留已有 servers，仅添加 `notegen` 项并验证格式。

连接前运行 `--check`。连接后查看工具列表并调用只读的 `notegen_get_status`、`notegen_list_notes` 测试。当前 server 不提供 HTTP URL，不应配置远程 connector URL。

卸载时移除单个 NoteGen MCP 项并删除程序目录；不要删除 NoteGen 工作区。若启动失败，查看桌面客户端日志和 server stderr，确认配置路径使用双反斜杠或 UI 可接受的 Windows 路径格式。

