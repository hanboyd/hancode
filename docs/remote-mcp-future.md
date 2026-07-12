# 未来远程 MCP 设计（当前未实现）

当前二进制只组合 `StdioTransport`，不创建 socket。未来增加独立 `cmd/notegen-mcp-http/` 与 `internal/transport/streamable_http.go`，复用现有 application services 和 workspace adapter；STDIO 与 HTTP 由不同 composition root 启动，可分别关闭。

HTTP 入口必须先经过 TLS 终止、Bearer/OAuth authentication、主体构造、authorization、按主体/工具速率限制，再进入 tool handler。身份信息通过 context 传递到审计，不允许由客户端参数伪造。Token capability 明确区分只读、单工作区写入和管理员，并按 tool、路径和批量上限取交集。

每个用户映射到独立的服务端 workspace handle，禁止客户端提交服务器绝对路径；租户根、索引、日志、回收站和 Git 凭据完全隔离。服务端不能直接暴露目录、静态文件或数据库。远程附件通过限流流式上传、内容类型/扩展名/大小校验和恶意内容隔离进入暂存区。

TLS 最低版本、反向代理信任边界、Host/Origin、防重放和请求体限制必须显式配置。远程审计记录主体、token id 哈希、来源、request id 和结果，不记录 token 或正文。备份与回滚仍使用工作区内临时备份和本地 Git checkpoint；远端 push 是单独、默认关闭的权限。

提供 `remote.enabled=false` 总开关和独立二进制，关闭后不绑定端口。STDIO 与 HTTP 可共享同一 application package，但不能共享未经协调的进程内锁；并存模式需引入跨进程锁或单写入协调器。

