# Remote Mic · RC003 Windows 1.0.1

这是 1.0.0 之后的 bugfix 版本，没有新增用户功能。普通用户继续使用已经通过
真实 RC003、VB-CABLE 与 Typeless 验收的 Python 协调器。

## Fixed

- 语音边沿 worker 在 BLE 断连/重连时不再复用带旧 sentinel 的队列：
  `eb1c919`（event-only 停止 + 每次重连换新的有界队列）。
- 低层键盘钩子不再对"不可能存在 suppression arm"的方向键事件做最长 60 ms
  的同步等待：`b9a45c6`。RC003 与普通实体键盘方向键长按恢复到正常约 31 ms
  重复，松开立即停止，不再出现松开后继续排空的 straggler。
- 改绑（custom → identity）后清除 stale armed-VK 状态，快路径立即恢复。

## Validation

- 原生绑定重建并验证为 `1.0.1`；Release ctest 51/51。
- 完整构建门通过（公开边界扫描、1106 项测试、PyInstaller、dry-run 与 Qt
  runtime smoke）。
- 安装版实机验收：RC003 方向键与普通实体键盘长按正常、松开立即停止；
  OK 正常；语音键无探针 smoke 3/3 通过。

## Known / Deferred

- 返回 -> Delete、音量上 -> Ctrl+C、音量下 -> Ctrl+V 物理修复：deferred
  （仅完成诊断）。
- HID-tap-active suppression eligibility：deferred。
- 语音快捷键 physicalization active-chain 调查：deferred。

千问集成、原生 WASAPI 无声问题以及代码签名仍不属于本版通过边界。
安装器和便携包均为 unsigned。请先用随包的 `SHA256SUMS.txt` 校验文件。
