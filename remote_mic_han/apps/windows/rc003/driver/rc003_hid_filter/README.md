# RC003 HID 载体键重映射筛选器

> 状态：**已实现并完成实体设备验证，分发暂缓。** 重映射链路于 2026-09-06 通过实体硬件验收（禁用 Frida tap、开启 HVCI，普通键盘不受影响）。它**不属于** RemoteMic 正式发布包；A/B/C 分发决策请见 `docs/ai_context/RC003-THREE-BUTTON-DISTRIBUTION-OPTIONS.md`。诊断控制设备 `\\.\Rc003HidCapture` 的修复已完成离线验证；其实体打开验证将在下一轮驱动安装时进行。

这是 RC003 蓝牙低功耗键盘 TLC 的下层筛选器。它以原位、等长度方式改写 kbdhid 无法转换为标准键盘用法的三个键盘页用法（载体键）：

- 0x0080（音量加）→ 0x0068（F13）
- 0x0081（音量减）→ 0x0069（F14）
- 0x00F1（返回）→ 0x006A（F15）

仅会修改报告 ID 1 中匹配的 16 位槽位；报告 ID、长度和其余所有字节均保持不变。不匹配的报告以字节完全一致的方式透传（故障开放）。不执行抑制或注入。采集环仍会记录原始线上字节，用于硬件验证。

在现有绑定/解析管线之前，RemoteMic 会将 RC003 专用的载体虚拟键（F13/F14/F15）还原为逻辑 `volume_up` / `volume_down` / `back` 按键，因此已保存的绑定无需改动。

## 挂载位置

RC003 键盘 TLC 设备节点（服务为 `kbdhid`）使用 `AddFilter` + `FilterPosition=Lower`：筛选器位于 **kbdhid 之下、HIDCLASS 集合 PDO 之上**，因此 `IRP_MJ_READ` 完成时可在 kbdhid 将用法转换为扫描码之前取得原始输入报告。

## 目录结构

- `src/driver.c`：DriverEntry（创建诊断控制设备、记录 QPC 频率）、EvtDeviceAdd（筛选器设置）和卸载。
- `src/read_capture.c`：`IRP_MJ_READ` 拦截、观察环，以及采集后的载体键重映射。
- `src/remap.c` / `src/remap.h`：纯粹、无内核依赖的用法替换。
- `src/control.c`：诊断控制设备 `\\.\Rc003HidCapture`（IOCTL 转储/清空）。
- `rc003_hid_filter.inf`：仅匹配 RC003 键盘 TLC 硬件 ID（带 PID 的服务 UUID 形式）的扩展 INF。
- `tools/rc003_capture_dump.py`：用户态转储工具。
- `tests/remap_fixtures.py`：以生产 `remap.c` 回放机器的历史原始报告（由 `run_remap_dll.bat` 构建为裸 DLL）。

## 构建（Enterprise WDK）

```bat
LaunchBuildEnv.cmd
SetupVSEnv
msbuild rc003_hid_filter.vcxproj /p:Configuration=Debug;Platform=x64
msbuild rc003_hid_filter.vcxproj /p:Configuration=Release;Platform=x64
```

产物位于 `bin\x64\<Configuration>\`。

## 安装与回退方案（构建步骤不会执行）

1. 开发机：关闭 Secure Boot，执行 `bcdedit /set testsigning on`，导入测试证书并重启。
2. 执行 `pnputil /add-driver rc003_hid_filter.inf /install`（在下次设备启动时，将下层筛选器应用到匹配的 RC003 键盘 TLC）。
3. 采集：在 RC003 上按返回、音量加、音量减、确认，再执行 `python tools\rc003_capture_dump.py --watch 1`。
4. 回退：执行 `pnputil /delete-driver rc003_hid_filter.inf /uninstall /force`（或 `oemNN.inf`），然后重启；设备将恢复原始堆栈。

## 采集约定

只保存含目标或对照用法的报告（最多 256 条，超出时丢弃最旧记录）；普通流量只增加计数器。绝不会记录用户文本。

## 诊断（控制设备）

`\\.\Rc003HidCapture` 由 **DriverEntry** 创建（KMDF 控制设备生命周期：`WdfControlDeviceInitAllocate` → `WdfDeviceCreate` → 符号链接 → 默认队列 → `WdfControlFinishInitializing`）。它的设计目标是诊断与故障开放：创建失败会通过 `DbgPrint` 记录，并写入驱动上下文中的 `ControlDeviceStatus`，但绝不会阻止筛选器挂载或重映射。

- `python tools\rc003_capture_dump.py [--clear] [--watch N]`：转储采集环；条目记录原始线上字节（采集在重映射前执行）。
- 若无法打开设备，转储工具会输出创建失败提示；驱动的 `DbgPrint` 跟踪包含准确的 NTSTATUS。
- 时间戳为 QPC tick；转储工具使用驱动在 DriverEntry 时从共享用户页复制的系统 QPC 频率将其转换为毫秒。
