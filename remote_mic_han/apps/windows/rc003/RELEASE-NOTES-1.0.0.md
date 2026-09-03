# Remote Mic · RC003 Windows 1.0.0

这是增量 C++ 重构后的首个可用混合版本。普通用户默认使用已经通过真实
RC003、VB-CABLE 与 Typeless 验收的 Python 协调器；原生协调器仍是诊断选项。

已验证：按下语音键后约 75 ms 发送一次 Typeless 快捷键，松开后约 75 ms
发送一次结束快捷键；PCM 有信号，完成一次输入后等待超过 30 秒没有自行再次
弹出 Typeless 窗口。

已知问题（按用户要求暂缓）：返回键 -> Delete、音量上 -> Ctrl+C、音量下 ->
Ctrl+V 的自定义映射当前不可用。配置仍然保留；新包和上一版本在本机都因
WUDFHost 返回 WinError 5（访问被拒绝）而无法启用实验性 HID tap。该问题不
影响 Typeless 验收，后续单独处理。

千问集成、原生 WASAPI 无声问题以及代码签名不属于本次 1.0.0 通过边界。

安装器和便携包均为 unsigned。请先用随包的 `SHA256SUMS.txt` 校验文件。
