# RemoteMic Windows

RemoteMic Windows bridges a Xiaomi Bluetooth Remote 2 Pro (RC003) to Windows voice-input applications. The imported and previously developed Windows product baseline lives under `apps/windows/rc003`; the C++20 project at the repository root establishes stable interfaces, diagnostics, and offline-test seams for later incremental migration.

## Windows 版本（RC003）

Windows 客户端位于 `apps/windows/rc003/`，当前定位为可继续开发和打包的源码/构建候选。历史源码记录中的真实硬件验收不能替代本仓库在用户自己的 RC003 到货后的重新验证；当前自动化也不能替代 BLE、HID、真实音频端点和目标语音应用验收。

## Current state

- Phase 0 framework: complete.
- Phase 1 product-baseline import and offline verification: complete.
- Phase 2 unsigned packaging candidate: complete; see `docs/baseline/CANDIDATE-ARTIFACTS.md`.
- Physical RC003 validation: deferred because hardware is not currently available.
- Historical source documentation reports an earlier real-device acceptance run, but this repository has not repeated it; current Bluetooth, HID, virtual microphone, Typeless, and Qianwen validation remains deferred.

## Build

From a Visual Studio Developer PowerShell:

```powershell
./scripts/build.ps1
./scripts/test.ps1
```

Prepare and test the imported Windows product baseline:

```powershell
./scripts/setup-baseline.ps1
./scripts/test-baseline.ps1
```

Build and package unsigned local candidates:

```powershell
./scripts/build-baseline-candidate.ps1
./scripts/package-baseline-portable.ps1
./scripts/package-baseline-installer.ps1
```

The installer must first be compiled from `apps/windows/rc003/installer/RemoteMicRC003Setup.iss` with Inno Setup. Generated candidates are written to the ignored `artifacts/` directory.

The CLI supports:

```powershell
./build/Debug/remotemic.exe --version
./build/Debug/remotemic.exe --diagnose
```

With a single-config generator, the executable can instead be under `build/` directly. The scripts locate the bundled CMake from Visual Studio Build Tools when it is not on `PATH`.

## Project map

- `apps/cli/`: minimal diagnostic CLI.
- `apps/windows/rc003/`: imported Python/PySide6 Windows product baseline.
- `include/remotemic/`: public C++ interfaces.
- `src/`: Phase 0 runtime-path, logging, and application skeleton.
- `tests/`: hardware-free unit tests and future fixtures.
- `docs/`: current project context, decisions, testing boundaries, and handover state.
- `tools/python/`: reserved for offline analyzers; Python is not part of the future real-time audio chain.

Local planning discussions under `讨论/` are intentionally excluded from Git.

## Source boundary

`remote_mic_han` is the active development and delivery repository. The separate `remote-mic-windows-port` folder is a read-only GitHub download containing Mac and Windows reference sources. Only the required Windows RC003 source, resources, CI, and license notices were imported here; the Mac source and downloaded release artifacts were not copied.
