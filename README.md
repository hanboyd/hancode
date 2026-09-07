# hancode

> RemoteMic Windows 的开发仓库：一款持续迭代的 Windows 无线麦克风项目。

[![RemoteMic](https://img.shields.io/badge/RemoteMic-Windows-1677ff?style=flat-square&logo=windows&logoColor=white)](remote_mic_han/README.md)
[![Release](https://img.shields.io/github/v/release/hanboyd/hancode?display_name=tag&sort=semver&style=flat-square)](https://github.com/hanboyd/hancode/releases)
[![Last commit](https://img.shields.io/github/last-commit/hanboyd/hancode?style=flat-square)](https://github.com/hanboyd/hancode/commits/main)

`hancode` 是 **RemoteMic Windows** 的开发仓库：它让小米蓝牙语音遥控器 2 Pro（RC003）接入 Windows 语音输入应用。

## 主项目：RemoteMic Windows

RemoteMic 将 RC003 变成紧凑的 Windows 语音输入控制器。仓库包含可运行的 Windows 基线产品、C++20 渐进式迁移基础、诊断工具、测试边界、打包脚本以及维护所需的设计文档。

| | |
|---|---|
| 当前源码候选版本 | **1.0.2** |
| 平台 | Windows 10 / 11 |
| 控制器 | 小米蓝牙语音遥控器 2 Pro（RC003） |
| 技术栈 | Python / PySide6 产品基线 + C++20 迁移层 |
| 验收边界 | 已提供离线与自动化检查；实体设备和目标应用验收须在目标机器上重新测试，当前仍明确暂缓。 |

从这里开始：[RemoteMic 项目说明](remote_mic_han/README.md) · [Windows RC003 客户端](remote_mic_han/apps/windows/rc003/README.md) · [版本说明](remote_mic_han/apps/windows/rc003/RELEASE-NOTES-1.0.2.md)

## 仓库结构

```text
hancode/
└── remote_mic_han/          RemoteMic Windows
    ├── apps/windows/rc003/  Windows 产品基线、测试与打包
    ├── include/ + src/      C++20 接口与迁移实现
    ├── docs/                架构、决策与交接上下文
    └── tests/               无硬件依赖的回归测试
```

## 快速开始：RemoteMic 开发

在 RemoteMic 项目目录中打开 **Visual Studio Developer PowerShell**：

```powershell
cd remote_mic_han
./scripts/build.ps1
./scripts/test.ps1
```

准备并测试导入的 Windows 产品基线：

```powershell
./scripts/setup-baseline.ps1
./scripts/test-baseline.ps1
```

构建和测试流程刻意不依赖硬件。蓝牙配对、HID 行为、虚拟音频路由以及真实语音应用交互，均需单独完成实体设备验收。

## 范围说明

- `remote_mic_han` 是当前维护中的项目，也是建议的起点。
- 生成的安装包和本地规划资料有意不纳入版本控制。

## 贡献与问题反馈

欢迎提交问题反馈和聚焦的拉取请求。RemoteMic 相关反馈请注明 Windows 版本、控制器/连接信息，并清楚区分自动化结果与实体设备观察结果。

---

由 [hanboyd](https://github.com/hanboyd) 开发与维护。
