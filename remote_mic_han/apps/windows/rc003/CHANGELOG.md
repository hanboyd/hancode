# Changelog — Remote Mic RC003 (Windows)

## [Unreleased]

### Phase 2 / Area 1 — ATVV capability parse（C++ 迁移）

完成 Phase 2 第 1 区（[ADR-0012](decisions/ADR-0012-atvv-adpcm-phase2-boundary.md)
第 3 节的 `remotemic::atvv::Capabilities`）。Python 基线
`ovb_rc003.atvv_protocol.ATVVCapabilities.parse` 与 C++ 实现
`remotemic::atvv::parse` 字节级一致。**Phase 2 全部 4 区尚未完成，版本号
不升**；下次小版本号 `0.3.0-candidate` 留给 Phase 2 closeout。

门禁（ADR-0012 §8）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 | `ctest -C Debug   -R '^remotemic_atvv_tests\$'`       | 1/1 通过 |
| G2 | `ctest -C Release -R '^remotemic_atvv_tests\$'`       | 1/1 通过 |
| G3 | `ctest -C Debug   -R '^remotemic_atvv_bind_smoke\$'`  | 1/1 通过 |
| G3 | `ctest -C Release -R '^remotemic_atvv_bind_smoke\$'`  | 1/1 通过 |
| G5 | `REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=shadow pytest tests/test_atvv_native_parity.py -q` | 2/2 通过，8 个夹具全部 byte-exact |

未跑 / 留待 Phase 2 全量完成：

- G4（Python 全量 ATVV 单元测试 100% 通过）— 已在 Phase 1 末尾
  验证（29 个测试 OK），Phase 2 期间未改动 Python 基线。
- G6（冻构建 --dry-run）— Phase 2 期间未重新打包；Phase 2 closeout
  前一次性跑。

新增内容：

- `include/remotemic/atvv/capabilities.hpp` — `remotemic::atvv::Capabilities`
  值类型 + `parse(std::span<const u8>) noexcept -> std::optional<Capabilities>`。
- `src/atvv/capabilities.cpp` — 与 Python 基线逐字节匹配的解析器。
- `tests/unit/test_atvv_capabilities.cpp` — CTest 单元测试，
  从同一套 JSON 夹具读 hex 与期望值。
- `tests/bind/test_atvv_bind_smoke.py` — pybind11 绑定烟雾测试。
- `tests/test_atvv_native_parity.py` — 运行时 shadow parity 测试
  （Python 与 C++ 字段全部 byte-exact，0 容差）。
- `apps/windows/rc003/src/ovb_rc003/atvv_native_bridge.py` —
  `python` / `native` / `shadow` 三态切换的薄包装；
  通过 `REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL` 环境变量控制。
- `apps/windows/rc003/tests/fixtures/atvv/` 新增 7 个 synthetic
  JSON 夹具：`synthetic-v1-8k-fallback.json` /
  `synthetic-v1-zero-frame-size.json` /
  `synthetic-v1-zero-codecs-quirk.json` /
  `synthetic-legacy-pre-1.0.json` /
  `synthetic-legacy-rejects-short.json` /
  `synthetic-wrong-opcode.json` /
  `synthetic-short-payload.json`。所有夹具 100% synthetic，
  无任何捕获的真实设备或语音数据。

行为变化：**无**。默认实现仍是 Python；只有 `native` 或 `shadow`
显式选择才会调用 C++ 路径。

---

## [0.2.0-candidate] — 2026-08-29 — Phase 1 complete

里程碑：9 阶段路线图第 1 阶段（C++/CPython 绑定骨架）完成且通过 exit review。

详细合同与门禁见
[`docs/decisions/ADR-0011-cpp-python-binding-and-error-model.md`](decisions/ADR-0011-cpp-python-binding-and-error-model.md) 与
[`docs/architecture/cpp-migration-execution-plan.md`](architecture/cpp-migration-execution-plan.md) 第 1 阶段。

### 新增（迁移基础设施，不动产品行为）

- 顶层 `CMakeLists.txt`：拆分出 `remotemic_core`（纯类型 + 错误类别）、
  `remotemic_platform_win32`（Win32 平台层）、`remotemic_bind`
  （INTERFACE 占位）、`remotemic_native_c`（pybind11 模块，输出名
  `_C`）。pybind11 v2.12.0 通过 FetchContent 固定到 CPython 3.11 venv
  shim，生成 `cp311-win_amd64` ABI 标签。
- `include/remotemic/bind/` + `src/bind/` + `tests/bind/`：
  `Error` / `ErrorCode` / `ErrorCategory`、`VersionInfo` / `Counter` /
  `CounterSink` 探针类型与 4 类别绑定烟雾测试。
- `apps/windows/rc003/src/remotemic_native/__init__.py`：公开 Python
  包装器（按 ADR-0011 第 50–51 行，`remotemic_native` 包对内提供
  `_C`，产品代码只通过公开包装器导入；`_C.pyd` 缺失时优雅降级到
  `None` 哨兵并设置 `_C_AVAILABLE = False`）。
- `apps/windows/rc003/src/ovb_rc003/_remotemic_native_runtime.py`：
  按 `REMOTEMIC_NATIVE_CHOICE_<MODULE_NAME>` 环境变量切换
  `python` / `native` / `shadow`；shadow 必须 `side_effect_free=True`
  且值不匹配或原生抛异常时抛 `RuntimeError`。
- `apps/windows/rc003/tests/test_remotemic_native_runtime.py`：
  11 个测试覆盖默认 python、环境变量覆盖、shadow 不匹配/异常、
  side-effect-free 约束；11/11 通过。
- `apps/windows/rc003/build/RemoteMicRC003.spec`：把整个
  `remotemic_native/` 包目录（含 `__init__.py` + `_C.pyd`）通过
  `datas=` 收进 `_internal/`，并把 `remotemic_native` 加入
  `hiddenimports`。不再以单 `_C.pyd` 暴露在 `_internal/` 根。
- `apps/windows/rc003/src/ovb_rc003/__main__.py`：`--dry-run` 按
  `_rn._C_AVAILABLE`（不是导入成功与否）分支；fallback 路径执行一次
  真实的 `ATVVCapabilities.parse()` 解析合成的 v1 caps payload，
  并打印 `version=0x0100 selected_codec=0x02 sample_rate=16000
  frame_size=120`。Gate 3 经冻构建（`dist/RemoteMicRC003/RemoteMicRC003.exe`
  双路径）实测通过。
- `.github/workflows/windows-rc003-ci.yml`：新增 `pull_request` /
  `push` paths filter（C++ 树、ADR、执行计划）以及
  `C++ core build + CTest (Debug + Release)` 步骤（在 VB-CABLE
  fetch 之后、PyInstaller 之前），跑 `remotemic_bind_smoke` 作为
  CTest 一部分。
- `.gitignore`：追加 `/Testing/`（CTest scratch）。

### 已知限制 / Phase 2 起跑线

- Phase 2 入口严格限定：ATVV capability parse、ATVV control message
  编解码、IMA/DVI ADPCM、格式与畸形输入处理。BLE、WASAPI、
  VoiceController/会话状态机、Windows 输入、Typeless/Qianwen、UI
  均不在 Phase 2 范围。
- 不升 `0.2.0` 之外的小版本号（per
  `memory/cpp-migration-version-policy.md` Rule 2，下一次小版本号
  升 `0.3.0` 留给 Phase 2 完成）。
- 本次不重新打包、不发布新安装器/便携 ZIP（per Rule 1）。

---

本项目按“候选发布”打标签。内部构建版本号固定在
`installer/RemoteMicRC003Setup.iss` 的 `AppVersion`（当前 `0.1.0-candidate`；随 ADR-0010
in-place installer upgrade 批次一起 bump，本 commit 不动），
仓库级 tag 只作为发布编号，两者对应关系以每条发布说明为准。

标签格式：`v<内部版本>-windows-rc003-candidate.<序号>`。

## [0.1.0-candidate] — 2026-07-31

标签：`v0.1.0-windows-rc003-candidate.1`（基于 `6c33fcc`）

首个 Windows RC003 候选发布。本版本已在真实 RC003 遥控器上完成逐键、
语音链路验收（详见 README“真机验收”部分）。CI 与自动构建仍然不能
替代真机验证。

### 新增

- **Frida HID tap 旁路**：对 Windows 普通输入链路拿不到的返回、音量+、
  音量- 缺失 usages（`0xF1`、`0x80`、`0x81`），复用上游
  `remote-bridge-hub` 的 Frida Gadget WUDFHost tap 读取；扩展为上报
  遥控器全部键盘 usage，作为所有普通按键的输入旁路。Gadget 是可选的
  第三方二进制，需显式获取（`build/fetch-frida-gadget.ps1`）且验证
  固定 SHA-256 后才会启用。
- **豆包语音触发（DoubaoPhysicalizer）**：注入的右 Alt 合成事件此前被
  豆包输入法 `ImeService` 的低层键盘钩子以 `LLKHF_INJECTED` 标志忽略；
  现在附加到 `ImeService.exe` 的低层回调，只对该标记事件清除 injected /
  lower-integrity 标志并清空 `dwExtraInfo`，使豆包看到的按键形状与
  实体右 Alt 一致。默认按住模式 `ralt`、切换模式 `ralt+space`。
- **设置页独立入口**：`RemoteMicRC003Settings.exe` 与桥接 EXE 分离
  （后合并为单个 EXE，见下）。
- **按键采集/回放工具**：`src/rc003_key_test.py`、`rc003_key_probe.py`
  等诊断工具，被动记录真实物理签名，不执行映射动作。

### 修复

- **普通按键双触发**：方向键、OK 等按键按下时一次动作被触发两次。
  根因是低层键盘钩子阻塞了 `WM_INPUT` 派发，导致“先 arm 后吞键”的
  等待式方案永远慢半拍。改为由 Frida GATT tap 的独立 socket 线程在
  `NtDeviceIoControlFile` 报告到达时 arm，低层钩子零等待匹配并吞掉
  原生按键，只注入一次映射动作。方向/OK/Home/Menu/TV/Power/返回/
  音量键全部实测通过。
- **F5 语音键重复替换刷屏**：按住麦克风键期间键盘 auto-repeat 会让
  “替换为右 Alt”逻辑反复触发；为 transform 增加已按下/已发送守卫，
  只在真实按下/释放边沿各发送一次。
- **BLE GATT 特征找不到**：修复后反复出现
  `ATVV characteristic not found`；改用 `BluetoothCacheMode.UNCACHED`
  读取服务与特征，避免 Windows 缓存旧枚举结果。
- **设置保存失败且映射不生效**：配置文件改为临时文件 + fsync +
  `os.replace` 原子写入；Qt 设置保存捕获一切持久化/回读异常并在界面
  显示错误；桥接进程在按键前按 mtime 热加载新的按键映射，磁盘数据
  损坏时保留最后一份有效映射。
- **启动闪黑色命令行窗口**：桥接启动子进程与打包运行时的控制台子进程
  均使用 `CREATE_NO_WINDOW` 隐藏。
- **语音识别无声/不稳定**：语音输出改为按端点能力输出立体声并复制
  声道；解码后增加 20 Hz 一阶高通 DC 阻挡；默认增益提高到 +10 dB；
  16 kHz → 48 kHz 改有状态连续插值（对齐上游）。实机验收：豆包输入法
  能识别遥控器语音。

### 变更

- **单 EXE 行为**：合并为同一个 `RemoteMicRC003.exe`。双击（无参数）或
  `--settings` 打开设置窗口；`--bridge` 显式启动桥接进程。安装器/便携版
  的启动快捷方式统一使用 `--bridge`。
- **设置保存原子化**：`save_config` / `save_key_bindings` 走原子写入，
  不暴露半写的 JSON。
- **返回键默认映射**：保持 `delete_backward`（退格）语义；新增可选的
  “浏览器后退”动作供用户在设置页手动绑定。
- 普通按键仍通过 `SendInput` 注入映射动作；语音快捷键通过物理化的
  右 Alt 事件；两者互不混用。

### 已知限制

- 未签名，首次运行会触发 SmartScreen 提示，属预期行为。
- Frida Gadget 与 VB-CABLE 均为可选第三方组件；未显式获取/安装时，
  缺失 usages 不会被猜测伪造，语音默认没有虚拟麦克风路由。
- 遥控器没有独立物理静音键；“系统静音”只是可选手动绑定。
- 安装器与便携版运行期配置都写入 `%LOCALAPPDATA%\RemoteMic\RC003`，
  卸载不会自动删除。
