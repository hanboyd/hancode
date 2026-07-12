# NoteGen MCP Server

Go 编写的本地高权限 NoteGen MCP Server。当前版本只通过 STDIO 操作显式配置的 Markdown 工作区。

**当前版本不会监听网络端口，也不会将 NoteGen 数据发送到远程服务器。**

## 状态

针对本机 NoteGen 0.31.0 调查并支持 0.31.x Markdown 工作区。已实现：状态、分页列出、读取整篇/行/章节、原子创建、带 `expected_hash` 的安全更新、路径边界、强制审计和 STDIO 工具发现。

尚未完成：SQLite 持久化全文索引、search、move、软删除/恢复、批量事务/回滚、标签/附件写入、Git checkpoint、watcher、diagnose/reindex 命令和完整性能基准。碎片/Todo 数据库写入因 NoteGen 同步一致性语义未验证而保持禁用。

## 架构与安全

Transport → MCP handler → application/workspace service → verified Markdown storage。索引只能是缓存。所有外部路径均相对于配置根；不允许永久删除。更新必须先读取最新内容并携带 hash，冲突返回 `VERSION_CONFLICT`。写操作记录不含正文的 JSONL 审计；审计失败会回滚写入。

## 构建

需要 Go 1.26.5：

```powershell
./scripts/test.ps1
./scripts/build.ps1
```

构建使用 `CGO_ENABLED=0`，无需额外运行时。输出位于 `dist/` 并含 SHA-256。官方 MCP Go SDK 固定为 v1.6.1。

## 配置与运行

复制 `config/config.example.json` 到 Git 忽略的私有路径，填入已存在的绝对目录：

```powershell
notegen-mcp.exe --config C:\secure\notegen-mcp.json --check
notegen-mcp.exe --stdio --config C:\secure\notegen-mcp.json
```

STDIO 下 stdout 仅供 MCP 协议；错误写 stderr，审计写文件。Codex 与 ChatGPT 示例见 `docs/`。

## MCP Tools

- `notegen_get_status`（只读）
- `notegen_list_notes`（只读、分页）
- `notegen_read_note`（只读、行/章节/截断）
- `notegen_create_note`（写入、原子创建）
- `notegen_update_note`（写入、乐观并发）

## 回收站、Git 与索引

配置已预留独立 trash/index/log 目录并排除普通工具访问；软删除/恢复、Git checkpoint 和 SQLite 中文 N-gram 索引仍在实现中，当前不能依赖这些能力。远程扩展入口见 `docs/remote-mcp-future.md`，当前仓库没有 HTTP server 实现。

## 升级与卸载

升级前备份 exe、私有配置和审计日志，运行新版本 `--check` 后再替换。卸载只移除客户端单个 MCP 条目和程序目录；不要删除 NoteGen 工作区。当前限制和验证事实见 `docs/research.md`。
