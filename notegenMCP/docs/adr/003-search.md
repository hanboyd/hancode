# ADR 003：中文全文搜索

状态：接受。

SQLite 使用纯 Go `modernc.org/sqlite`，不依赖 CGO。索引保存标准化正文，并额外保存连续 Unicode rune 的 2-gram/3-gram 预分词 token。查询同样预分词后先以 token 缩小候选，再对原文做精确短语/多关键词校验和片段定位。

未选择默认 `unicode61`，因为单个连续中文句子常被当成一个 token；未把 FTS5 trigram 作为唯一方案，因为构建环境和 SQLite 编译选项可能变化。应用层 N-gram 行为可测试、可降级，代价是索引体积增加。索引不可用时允许受限慢速扫描。

