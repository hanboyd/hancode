# RemoteMic Windows

RemoteMic Windows 将小米蓝牙语音遥控器 2 Pro（RC003）接入 Windows 语音输入应用。已导入并持续维护的 Windows 产品基线位于 `apps/windows/rc003`；仓库根目录的 C++20 项目提供稳定接口、诊断能力与离线测试边界，用于后续渐进式迁移。

## Windows 版本（RC003）

Windows 客户端位于 `apps/windows/rc003/`，当前定位为可继续开发和打包的源码/构建候选。历史源码记录中的真实硬件验收不能替代本仓库在用户自己的 RC003 到货后的重新验证；当前自动化也不能替代 BLE、HID、真实音频端点和目标语音应用验收。

## 当前状态

- 第 0 阶段框架：已完成。
- 第 1 阶段产品基线导入与离线验证：已完成。
- 第 2 阶段未签名打包候选：已完成，见 `docs/baseline/CANDIDATE-ARTIFACTS.md`。
- RC003 实体设备验证：因当前没有硬件而暂缓。
- 历史源码文档记录过一次实体设备验收，但本仓库尚未复现；当前蓝牙、HID、虚拟麦克风、Typeless 与千问的验证仍暂缓。

## 构建

在 Visual Studio Developer PowerShell 中运行：

```powershell
./scripts/build.ps1
./scripts/test.ps1
```

准备并测试导入的 Windows 产品基线：

```powershell
./scripts/setup-baseline.ps1
./scripts/test-baseline.ps1
```

构建并打包本地未签名候选：

```powershell
./scripts/build-baseline-candidate.ps1
./scripts/package-baseline-portable.ps1
./scripts/package-baseline-installer.ps1
```

安装器须先使用 Inno Setup 从 `apps/windows/rc003/installer/RemoteMicRC003Setup.iss` 编译。生成的候选产物会写入被忽略的 `artifacts/` 目录。

命令行工具支持：

```powershell
./build/Debug/remotemic.exe --version
./build/Debug/remotemic.exe --diagnose
```

使用单配置生成器时，可执行文件可能直接位于 `build/`。当 `PATH` 中找不到 CMake 时，脚本会定位 Visual Studio Build Tools 随附的版本。

## 项目结构

- `apps/cli/`：精简的诊断命令行工具。
- `apps/windows/rc003/`：导入的 Python/PySide6 Windows 产品基线。
- `include/remotemic/`：公开的 C++ 接口。
- `src/`：第 0 阶段运行路径、日志与应用骨架。
- `tests/`：无硬件依赖的单元测试与后续夹具。
- `docs/`：当前项目上下文、决策、测试边界与交接状态。
- `tools/python/`：预留给离线分析工具；Python 不属于未来实时音频链路。

`讨论/` 下的本地规划讨论有意不纳入 Git。

## 源码边界

`remote_mic_han` 是当前开发与交付仓库。独立的 `remote-mic-windows-port` 文件夹是只读的 GitHub 下载内容，包含 Mac 与 Windows 参考源码。这里只导入了所需的 Windows RC003 源码、资源、CI 和许可证声明；没有复制 Mac 源码或下载的发布产物。
