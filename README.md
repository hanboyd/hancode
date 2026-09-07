# hancode

> A practical collection of small tools and an actively developed Windows remote-microphone project.

[![RemoteMic](https://img.shields.io/badge/RemoteMic-Windows-1677ff?style=flat-square&logo=windows&logoColor=white)](remote_mic_han/README.md)
[![Release](https://img.shields.io/github/v/release/hanboyd/hancode?display_name=tag&sort=semver&style=flat-square)](https://github.com/hanboyd/hancode/releases)
[![Last commit](https://img.shields.io/github/last-commit/hanboyd/hancode?style=flat-square)](https://github.com/hanboyd/hancode/commits/main)

`hancode` is my working repository for useful experiments, learning projects, and software I continue to maintain. The current focus is **RemoteMic Windows**: a Windows client that connects a Xiaomi Bluetooth Remote 2 Pro (RC003) to voice-input applications.

## Featured project — RemoteMic Windows

RemoteMic turns the RC003 into a compact voice-input controller for Windows. The repository contains the production Windows baseline, a C++20 migration foundation, diagnostics, test seams, packaging scripts, and the design notes needed to keep the project maintainable.

| | |
|---|---|
| Current source candidate | **1.0.2** |
| Platform | Windows 10 / 11 |
| Controller | Xiaomi Bluetooth Remote 2 Pro (RC003) |
| Stack | Python / PySide6 product baseline + C++20 migration layer |
| Validation boundary | Offline and automated checks are available; physical-device and target-app acceptance remain explicitly deferred until re-tested on the target machine. |

Start here: [RemoteMic documentation](remote_mic_han/README.md) · [Windows RC003 app](remote_mic_han/apps/windows/rc003/README.md) · [release notes](remote_mic_han/apps/windows/rc003/RELEASE-NOTES-1.0.2.md)

## Repository map

```text
hancode/
├── remote_mic_han/          RemoteMic Windows — active project
│   ├── apps/windows/rc003/  Windows product baseline, tests, and packaging
│   ├── include/ + src/      C++20 interfaces and migration implementation
│   ├── docs/                Architecture, decisions, and handover context
│   └── tests/               Hardware-free regression coverage
├── expense_tracker/         Small Python expense-tracking exercise
├── markdown_todo_manager/   Markdown-backed command-line to-do manager
└── notegenMCP/              Historical NoteGen MCP source (no longer maintained here)
```

## Quick start: RemoteMic development

Use a **Visual Studio Developer PowerShell** from the RemoteMic project directory:

```powershell
cd remote_mic_han
./scripts/build.ps1
./scripts/test.ps1
```

To prepare and test the imported Windows product baseline:

```powershell
./scripts/setup-baseline.ps1
./scripts/test-baseline.ps1
```

The build and test workflow is deliberately hardware-free. Bluetooth pairing, HID behavior, virtual audio routing, and real voice-app interaction need their own on-device acceptance pass.

## Notes on scope

- `remote_mic_han` is the maintained project and the best place to begin.
- The smaller top-level folders are self-contained learning or utility projects; read their local files before treating them as active products.
- Generated packages and local planning material are intentionally excluded from version control.

## Contributing and issues

Bug reports and focused pull requests are welcome. For RemoteMic work, please include Windows version, controller/connection details, and a clear distinction between automated results and real-device observations.

---

Built and maintained by [hanboyd](https://github.com/hanboyd).
