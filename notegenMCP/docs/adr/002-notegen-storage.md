# ADR 002：NoteGen 存储适配

状态：接受。

文章以配置指定的 Markdown 工作区为事实来源。NoteGen 0.31.0 的 `note.db` 保存碎片、Todo、标签等应用数据；第一阶段只读访问已由对应 tag 源码验证的 schema，不直接写数据库。独立索引只是可重建缓存。

