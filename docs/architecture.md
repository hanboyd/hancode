# 架构

依赖方向固定为：Transport → MCP handlers → Application services → `NoteGenWorkspace` → verified Markdown storage。SQLite 索引、Git、审计和文件系统安全均由应用服务通过小接口组合；领域层不依赖 MCP SDK。

当前唯一启动传输为 STDIO。`main` 不创建 listener，也不导入 HTTP server。授权器只允许配置根目录内的本地调用。未来传输和身份认证通过接口注入，不改变笔记服务。

写入边界：路径解析 → 授权 → 文件锁 → 版本检查 → 同目录临时文件 → flush → 原子替换 → 哈希验证 → 索引 → 审计。批量服务在执行前生成完整计划与备份，失败按逆序回滚。

