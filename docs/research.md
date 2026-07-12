# NoteGen 本机调查报告

调查日期：2026-07-12（Asia/Shanghai）  
调查方式：只读检查本机注册信息、进程、目录结构和 NoteGen `note-gen-v0.31.0` 官方源码。未读取、修改或输出笔记正文；未写入 `note.db`。

## 结论摘要

- 当前安装版：**NoteGen 0.31.0**。Windows 卸载注册表另有 0.30.1 残留项；运行文件的 PE 资源版本 `0.1.0` 不可信，安装记录、官方 release 和对应源码 tag 一致指向 0.31.0。
- 安装位置：`C:\Users\hanboyd\AppData\Local\NoteGen\note-gen.exe`。
- 应用数据：`C:\Users\hanboyd\AppData\Roaming\com.codexu.NoteGen`。
- 当前自定义 Markdown 工作区：`C:\Users\hanboyd\hanNoteGen`（来自 `store.json` 的 `workspacePath`）。
- 默认 Markdown 目录（当前未采用）：应用数据目录下的 `article/`。
- 碎片、Todo、碎片标签：应用数据目录下 `note.db`，SQLite WAL 模式。
- 碎片本地资产：`image/`、`screenshot/`、`recordings/`；外部文件型碎片也可能保存 URL/路径。
- NoteGen 当前进程未监听 TCP 端口。
- 未发现稳定的对外 CLI、本地 HTTP 服务或供外部进程调用的 NoteGen API。
- 官方源码有 Tauri commands，但属于进程内 WebView IPC，不是稳定的跨进程接口。
- 文章编辑器直接使用 Tauri filesystem plugin 读写 Markdown；因此文章能力首选受限的 Markdown 工作区适配器。

## 数据模型与存储

### Markdown 文章

`src/lib/workspace.ts` 表明：若 `store.json.workspacePath` 存在，NoteGen 直接以该绝对目录作为工作区；否则使用 `BaseDirectory.AppData/article`。文章树递归枚举文件，编辑器通过 `readTextFile`/`writeTextFile` 直接读写。

源码没有规定文章必须带 frontmatter，也未发现 NoteGen 专有 frontmatter schema。文件名/目录即主要组织结构；MCP 必须保留现有 Markdown、换行和 BOM，不应自行重排 frontmatter。

### 碎片记录与 Todo

`note.db` 由 `@tauri-apps/plugin-sql` 以 `sqlite:note.db` 打开。已在 0.31.0 源码验证的核心表：

- `marks(id, tagId, type, content, url, desc, deleted, createdAt)`
- `tags(id, name, isLocked, isPin, sortOrder)`
- `notes(id, tagId, content, locale, count, createdAt)`（历史/生成内容表，不等同于 Markdown 文件树）

`marks.type` 已验证值：`scan | text | image | link | file | recording | todo`。Todo 数据编码在 `marks` 中；更细的 Todo 字段由应用层编码，第一阶段不在未完成兼容性验证前直接写表。

删除碎片时 NoteGen 通常把 `deleted` 设为 1；永久删除才删除行和本地资产。MCP 的 Markdown 回收站是独立机制，不能冒充 NoteGen 数据库回收站。

### 标签与元数据

碎片标签存放在 `tags` 表，通过 `marks.tagId` 关联。Markdown 文章标签没有在源码中发现统一的 NoteGen 专有格式；若存在 YAML frontmatter，MCP 可解析常见 `tags` 字段，但写入前应保留原格式。不得把数据库碎片标签与文章 frontmatter 标签混为一谈。

### 图片与附件

碎片资产规则已验证：

- `scan` → AppData `screenshot/<filename>`
- `image` → AppData `image/<filename>`
- `recording` → AppData 下存储的规范化相对路径（通常 `recordings/`）
- HTTP(S) URL 不视为本地资产

Markdown 图片路径由编辑器按工作区相对路径处理；当前工作区的具体附件布局不是固定 schema，应通过引用扫描和配置策略发现，不能猜测单一附件目录。

## Git、同步与刷新行为

NoteGen 0.31.0 支持 GitHub、Gitee、GitLab、Gitea、S3 和 WebDAV 同步。Git 平台同步由前端服务层/API 实现，并不提供一个可复用的本地 Git CLI 接口。凭据可能存在 `store.json`，MCP 不读取、不复制、不记录这些值。

源码显示文章树会在界面动作及同步事件后重新加载，但未找到可靠的通用操作系统文件 watcher。故不能保证 NoteGen 对任意外部修改即时自动刷新；用户可能需要切换文件/目录或重新打开。MCP 自身必须使用 watcher 加周期一致性检查，并始终在写入前重新读取文件。

NoteGen 编辑器和 MCP 同时写文件存在丢失更新风险。MCP 必须使用内容哈希/修改时间做乐观并发控制、文件级锁和原子替换；NoteGen 本身的普通 `writeTextFile` 不提供跨进程 compare-and-swap。Git/云同步也可能在读取后改变文件。

未发现 NoteGen 自动规范化或重写所有 Markdown/frontmatter 的证据；编辑器保存可能按其 Markdown 编辑器序列化结果改写当前文档。因此 MCP 应进行最小文本修改，并验证最终哈希。

## 接口调查

| 候选接口 | 结论 | 第一阶段策略 |
|---|---|---|
| 官方稳定外部接口 | 未发现 | 不依赖 |
| 内部前端服务层 | 有，但绑定 Tauri/TypeScript 运行时 | 仅作为行为依据 |
| Markdown 工作区 | 已验证、直接读写 | 文章的事实来源 |
| `note.db` | schema 可由源码验证，运行中为 WAL | 碎片/标签先只读；写入需单独兼容性验证 |
| Tauri Commands | 有，进程内 IPC | 不作为外部 API |
| 本地服务/HTTP | 当前进程未监听端口，源码未发现文章 API | 不使用 |
| CLI | 未发现 | 不使用 |
| Rust 服务层 | 有 AI、备份、MCP runtime 等命令，无稳定文章存储 API | 不复用二进制接口 |

## 安全实施决策

1. 配置只接受显式根目录，不自动扫描硬盘。
2. 当前真实工作区仅用于 `--check`/`--diagnose` 等只读验证；测试全部使用临时目录。
3. Markdown 写能力只针对配置根内文件，经过 Windows 路径、重解析点和最终句柄路径校验。
4. 不向 NoteGen `note.db` 插入或更新记录；碎片和 Todo 的写能力在验证事务、同步队列和 UI 刷新语义前标记为未实现。
5. MCP 索引是独立缓存，不修改 NoteGen 数据库。
6. 不读取 `store.json` 中的 token、模型密钥或同步凭据。

## 证据与版本

- NoteGen 官方仓库 tag：`note-gen-v0.31.0`
- 源码 commit：`b3813b4cf5d3013b5b9be75b9b393daa7bcdabeb`
- 技术栈：Tauri 2、Next.js/React、Rust commands、Tauri SQL/Store/FS plugins
- 本机 `note.db` 同时存在 `-wal` 与 `-shm`，说明应用运行时采用 WAL。
- 调查时 NoteGen 进程没有 TCP listener。

## 尚待运行期验证

- 外部修改后 UI 自动刷新的确切时延和所有触发条件。
- 自定义工作区内附件目录约定（按实际引用统计，不读取正文输出）。
- Markdown 编辑器对各种 frontmatter、BOM、CRLF 的保存行为。
- 碎片 Todo 的完整 JSON/文本编码以及同步队列的一致性要求。
- NoteGen 升级后的 schema migration 兼容范围。

这些项目不会通过猜测补齐；无法确认的数据库写操作保持禁用。
