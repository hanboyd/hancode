# 安全模型

admin 仅表示配置工作区内的笔记管理权限。它不授权 shell、永久删除、系统设置、凭据、浏览器数据或任何工作区外路径。配置拒绝 `allow_permanent_delete=true`。

路径校验拒绝绝对路径、UNC、盘符、ADS、`..`、Windows 设备名、非法字符、尾空格/句点；解析 symlink/junction 后再次用 `filepath.Rel` 验证归属，并排除 trash/index/log 目录。写入使用哈希并发检查、细粒度锁、同目录临时文件、flush、替换和强制 JSONL 审计。

