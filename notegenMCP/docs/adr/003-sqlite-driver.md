# ADR 003：纯 Go SQLite 驱动

状态：接受。

选择 `modernc.org/sqlite` v1.39.1。它把 SQLite 的 C 实现转换为 Go，运行和交叉构建不需要 C 编译器，能够在 `CGO_ENABLED=0` 下生成 Windows amd64 单一 exe。项目使用其 `database/sql` 驱动接口，业务层不依赖驱动专有 API。

代价是 exe 和首次编译缓存明显增大，某些极端写入负载可能弱于原生 CGO 驱动；NoteGen MCP 的工作负载以增量文档索引和本地查询为主，部署简单性与可重复性优先。驱动采用 BSD-3-Clause；SQLite 本体为 public domain。

索引 schema 带整数版本。兼容迁移使用显式事务；未知的新版本返回 `INDEX_CORRUPTED/INDEX_UNAVAILABLE`，绝不静默删除。完整重建写入同目录临时数据库、执行 integrity check、关闭并 flush 后再替换。索引只是缓存，任何失败均不得修改 Markdown。

