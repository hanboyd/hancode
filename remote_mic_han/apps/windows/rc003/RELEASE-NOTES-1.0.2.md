# Remote Mic · RC003 Windows 1.0.2

这是 1.0.1 之后的维护版本，没有新增用户功能。普通用户继续使用已经通过
真实 RC003、VB-CABLE 与 Typeless 验收的 Python 协调器；安装体验与 1.0.1
完全一致：不请求管理员权限、不安装任何驱动、不修改 Secure Boot /
TESTSIGNING / BCD、不导入任何证书。

## What's new for users

- 版本号更新为 1.0.2（内部构建版本号，与 Release 的仓库级 tag 无关，见
  README 的"获取构建产物"说明）。
- 随包文档更新了 RC003 返回 / 音量+ / 音量− 三键的状态说明（见下方
  Known limitations / Development status）。运行时代码行为与 1.0.1 相同。

## Known limitations / Development status

- RC003 的返回、音量+、音量− 三个物理键在本正式包中仍不可用：
  Windows 的 kbdhid 不翻译这三个键盘页 usage，正式包保持正常用户态路径，
  不包含任何 HID filter。
- 底层修复方案已经开发完成并通过实机验证：设备级 carrier-remap HID
  filter（`apps/windows/rc003/driver/rc003_hid_filter`）在 2026-09-06 的
  真实硬件验收中通过（Back → Delete、Volume+ → Ctrl+C、Volume− → Ctrl+V；
  Frida tap 明确禁用；HVCI 保持开启；普通键盘不受影响）。
- 该 HID filter 暂不进入正式发行包，原因是 driver distribution/signing
  尚未进入正式发布路径（正式分发需要生产签名，当前 deferred）。
- **普通用户无需、也不应为此修改 Secure Boot 或 TESTSIGNING，不要进入
  Test Mode。** 这三键的修复会随未来签名的 driver 发行恢复。
- 其他已知 deferred 项不变：千问集成、原生 WASAPI 无声问题、代码签名。

## Validation

- 版本同步：CMake / Python 元数据 / installer AppVersion / binding smoke
  锁步为 `1.0.2`。
- Debug / Release 原生构建 + ctest 全绿；完整 Python 测试套件通过；
  packaging contract 测试通过；安装器与便携包构建通过；包内容审计确认
  正式包不含 rc003_hid_filter 任何产物（sys/INF/catalog/证书/脚本）。

千问集成、原生 WASAPI 无声问题以及代码签名仍不属于本版通过边界。
安装器和便携包均为 unsigned。请先用随包的 `SHA256SUMS.txt` 校验文件。
