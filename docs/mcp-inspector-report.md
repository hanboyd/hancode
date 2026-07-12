# MCP Inspector 验证报告

日期：2026-07-12  
Inspector：官方 `@modelcontextprotocol/inspector` 0.21.2，Node.js 22.23.1，仅作为开发验证工具。NoteGen MCP 最终 exe 不依赖 Node。

## 启动方式

Inspector CLI 通过 STDIO 启动：

```powershell
mcp-inspector --cli --method tools/list -- `
  .\dist\notegen-mcp.exe --stdio --config .\.tmp\inspector-config.json
```

测试配置指向仓库 `.tmp/inspector-workspace`，未使用真实 NoteGen 工作区。Inspector CLI 自身没有为 NoteGen MCP 启动 HTTP transport；被测 exe 未监听端口。

## 结果

| 项目 | 结果 |
|---|---|
| initialize / initialized | 通过；否则 Inspector 无法进入 tools/list/call |
| server instructions | 通过 SDK 集成测试验证，首部包含相对路径、先读后写、expected_hash、回收站和 dry_run 规则 |
| tools/list | 通过，发现当前 5 个工具 |
| input/output schema | 通过，Inspector 返回 draft-compatible schema |
| annotations | 通过；只读、destructive、idempotent、openWorld 标记符合当前行为 |
| `notegen_get_status` | 通过 |
| `notegen_list_notes` | 通过 |
| `notegen_read_note` | 通过 |
| `notegen_create_note` | 通过，写入临时工作区并产生审计 |
| `notegen_update_note` | 通过，使用 create 返回的 SHA-256 |
| 稳定错误 | 通过，错误 hash 返回 `VERSION_CONFLICT` 和 current_hash |
| 客户端断开/进程退出 | 通过；每次 Inspector CLI 结束后无残留 notegen-mcp 进程 |
| context cancel | SDK 内存传输测试通过；CLI 断开会取消 server context |
| stdout 隔离 | 通过；Inspector 可解析全部 JSON-RPC，未见启动横幅/普通日志 |
| stderr | 未污染协议；配置或进程级错误仅写 stderr |
| TCP listener | 被测进程无 listener |

## 发现与修复

1. Inspector 参数必须在 `--` 前，server 的 `--config` 参数必须在 `--` 后，否则会被 Inspector 自身解析。
2. typed handler 在业务错误时返回零值 output，SDK v1.6.1 会为了满足声明的 output schema 补入零值 structured content；稳定错误仍位于 `content` 且 `isError=true`。尝试可空 output 后，SDK 按其设计仍替换为元素零值，因此记录为 SDK 限制，客户端必须在 `isError` 为真时忽略 structured content。
3. Inspector `--version` 不是只读版本命令，会进入 UI 启动路径；报告版本取已缓存 package manifest。

所有写入均发生于 `.tmp`，真实工作区未被读取或修改。
