# ADR-0015: Phase 5 — Windows Input + Host Action Sink C++ 迁移边界

- Status: **proposed**
- Date: 2026-08-31
- Phase: 5 (of 9, per `docs/architecture/cpp-migration-execution-plan.md`)
- Related: [ADR-0011](ADR-0011-cpp-python-binding-and-error-model.md)（Python/C++ 绑定与错误模型）、[ADR-0014](ADR-0014-phase4-audio-wasapi-cpp.md)（Phase 4 WASAPI AudioRoute；本 ADR 沿用 `IAudioRoute` 的"原子状态 + 无阻塞入队"原则）

## Context

Phase 0-4 已交付：协议核心 + ATVV/ADPCM 4 区 + 会话状态机 + bounded PCM 队列 + IAudioRoute WASAPI 后端 + Python/native 工厂切换；自动化门禁全部通过，version `0.5.0-candidate`。下一阶段按 `cpp-migration-execution-plan.md` §5 阶段 5，把**Windows 输入采集 + 低级键盘钩子 + 动作发送**迁入 C++ 基础设施层。

这一阶段的特殊性：

- 现有 `apps/windows/rc003/src/ovb_rc003/raw_input_windows.py`（1055 行）+ `legacy_key_suppressor_windows.py`（609 行）+ `win32_input.py`（547 行）+ `frida_hid_tap_*.py`（639 行）+ `hotkey.py` + `key_mapping.py` + `win32_keys.py` + `hid_identity.py` + `key_testing.py`，共 **3579 行 Python**。把"按键采集 + 低级钩子 + 动作发送"这三个职责揉在 Python + ctypes + Frida IPC + Raw Input Win32 API 里，hook 回调路径上同时跑 GIL 持有 + ctypes 调用 + Frida socket 读取；按 `cpp-migration-execution-plan.md` §3 rule 6 严格禁止钩子回调路径上的阻塞 I/O 或进程调用。
- "Back / volume-up / volume-down do not reach Raw Input or the low-level keyboard hook on this Windows host. Elevated WUDFHost injection and direct HID-over-GATT characteristic access are both denied by Windows." 是 Phase 3 / Phase 4 一直 carry-forward 的 deferred 项；本 ADR 直面这一缺口，但**不**许诺填补它——只是给出 C++ 端独立验证 Frida tap 数据流 / Raw Input 数据流的入口，并保留"deferred"作为最终退出门禁。
- `cpp-migration-execution-plan.md` §3 rule 5 明确禁止输入路径"双 owner"。Raw Input 与 Frida HID tap 不可同时把同一按键转发到应用协调层；同理 hook 抑制路径与 Frida tap 路径必须互斥拥有按键的生命周期。`shadow` 双跑**不允许**。
- `cpp-migration-execution-plan.md` §3 rule 6 强制钩子回调路径非阻塞：C++ 端 raw input callback / low-level keyboard hook callback 只能更新原子状态和无阻塞入队，绝不等待锁、音频、进程、文件或 Python GIL。BLE 回调 / 钩子回调路径中**不**允许调用任何"可能阻塞"的 Win32 API（`SendMessage` / `GetMessage` / `WaitForSingleObject` / `WriteFile` 等）。
- `cpp-migration-execution-plan.md` §3 rule 8 要求保留用户的 `key_bindings.json`，不得把 back / volume_up / volume_down 硬编码为固定动作；用户的手动键位映射表是产品数据，不是基础设施可改的常量。
- `cpp-migration-execution-plan.md` §3 rule 7 要求 Qianwen 物理化逻辑留在隔离适配器中；核心状态机不得依赖进程名或窗口标题。本 ADR 在接口层面固化这一约束。
- `cpp-migration-version-policy.md` Rule 2：本阶段 closeout 时版本号 `0.5.0-candidate → 0.6.0-candidate`，三处（CMakeLists.txt / Python `__version__` / `pyproject.toml`）lock-step bump；installer `AppVersion` 按 Rule 1 不动。

## Decision

### 1. Phase 5 范围（3 个子项，按 3 个独立 commit 提交）

| 子项 | Python 来源 | C++ 目标 | 类型 |
|---|---|---|---|
| Raw Input 解析 + Frida HID tap 适配 | `raw_input_windows.py`（1055 行）+ `frida_hid_tap_injector.py`（286 行）+ `frida_hid_tap_runtime.py`（353 行）+ `hid_identity.py`（202 行） | `remotemic::input::RawInputSource` + `remotemic::input::FridaHidTapSource` + `remotemic::input::InputEvent` | 纯类 + Windows-only 基础设施 |
| 低级键盘钩子 / 抑制 / 转发 | `legacy_key_suppressor_windows.py`（609 行） | `remotemic::input::LowLevelKeyboardHook` + `remotemic::input::KeySuppressor` | Windows-only 钩子 + 原子状态 |
| SendInput 动作发送 + hotkey 物理化 | `win32_input.py`（547 行）+ `hotkey.py`（107 行）+ `key_mapping.py`（303 行）+ `win32_keys.py`（117 行） | `remotemic::input::SendInputActionSink` + `remotemic::input::HotkeyPhysicalizer` + `remotemic::input::ActionResolver` | Windows-only 动作接收器 |

### 2. Phase 5 **非范围**（硬约束）

- UI（`settings_ui.py` / `qt_settings_app.py` / `qml/`）—— 一直保留 PySide6
- BLE / WinRT 传输（`ble_transport_winrt.py`）—— Phase 6
- Typeless / Qianwen 集成 bug —— 用户原话"先不管 bug"，核心状态机不得依赖进程名或窗口标题；Qianwen 物理化逻辑留在隔离适配器，**不**进入本 ADR 决策面
- Frida Gadget 获取 / 签名 / WUDFHost 注入的实际填补 —— Back / Volume+ / Volume- deferred；本 ADR 不许诺解决，仅提供 C++ 端独立验证 Frida tap 数据流是否到达 Frida IPC socket 的入口
- 键位映射表迁移 —— `key_bindings.json` 保持 Python 端读取；C++ 端只接受"已解析后的动作"，不读 JSON、不解析 user binding
- 跨进程 hook / global hook —— Windows UIPI 边界让 elevation 不一致时 hook 自动失效；本 ADR 不试图绕开
- 输入路径"双 owner"（Raw Input + Frida HID tap 同时声明同一按键）—— plan §3 rule 5 禁止
- 把 hotkey / keymap 写进 keymap 静态文件 —— plan §3 rule 8 用户数据不写

### 3. 模块合同

#### 3.1 `remotemic::input::InputEvent`（值类型，POD-ish）

```cpp
struct InputEvent {
    enum class SourceKind : std::uint8_t {
        RawInputKeyboard,
        RawInputHid,
        FridaHidTap,
        LowLevelHookKeyboard,
        Synthetic,  // virtual test source
    };
    enum class EventKind : std::uint8_t {
        KeyDown,
        KeyUp,
        KeyCancel,  // LL hook cancellation path (rare)
        SystemAction,  // volume / power / showDesktop / openCodex etc.
    };

    std::chrono::steady_clock::time_point timestamp{};
    SourceKind source{SourceKind::RawInputKeyboard};
    EventKind kind{EventKind::KeyDown};
    std::uint16_t vk_code{0};        // Windows VK_* (KeyDown / KeyUp only)
    std::uint16_t scan_code{0};
    std::uint32_t usage_id{0};       // HID usage page 0x07 / 0x0C / 0x01 etc.
    std::uint32_t extra_info{0};     // KBDLLHOOKSTRUCT dwExtraInfo (LL hook only)
    bool injected{false};            // KBDLLHOOKSTRUCT flags LLKHF_INJECTED
    bool extended{false};            // KBDLLHOOKSTRUCT flags LLKHF_EXTENDED
};
```

- 不变量：`vk_code == 0` 仅允许出现在 `SystemAction`（音量 / 电源 / showDesktop）；`KeyDown` / `KeyUp` 必须携带合法 VK。
- 不持有字符串 / `std::string` / 引用 —— 输入路径**绝不**分配。
- 线程安全：值类型，本身不可变；跨线程通过原子化传递或 `std::atomic<InputEvent>` 或 lock-free SPSC queue（实现阶段决定，接口层面不约束）。

#### 3.2 `remotemic::input::IInputSource`（输入源接口）

```cpp
class IInputSource {
public:
    virtual ~IInputSource() = default;

    // Register the callback. The implementation MUST NOT capture the
    // callback across a non-blocking boundary (atomic flag + lock-free
    // queue is the contract; no std::function / std::promise / future
    // crossings). The callback runs on the implementation's own thread;
    // it must return within a tight budget (default 5 us; see §5) and
    // never call back into the source.
    virtual void set_event_sink(void (*sink)(InputEvent, void*),
                                void* user_data) noexcept = 0;

    // Start the source. Idempotent. Returns true on success, false on
    // any platform error. Never throws. After start(), events flow
    // through ``sink`` until stop() or destruction.
    virtual bool start() noexcept = 0;

    // Stop the source. Idempotent. After stop(), no further callbacks
    // are invoked.
    virtual void stop() noexcept = 0;
};
```

- 单 owner 规则（plan §3 rule 5）：同一时刻只有**一个** `IInputSource` 处于 `started == true` 状态；hook / Raw Input / Frida HID tap 三选一。协调层负责切换。

#### 3.3 `remotemic::input::IHostActionSink`（动作接收器接口）

```cpp
class IHostActionSink {
public:
    virtual ~IHostActionSink() = default;

    // Submit a single keyboard action (VK + key-up). Batchable at the
    // implementation level; this interface submits one at a time and
    // the implementation MAY coalesce internally if it sees back-to-back
    // calls within a single frame (Phase 7 Application coordination will
    // decide). Returns true on queued, false on sink-down.
    virtual bool submit_key(std::uint16_t vk_code,
                            bool key_down,
                            std::chrono::milliseconds deadline) noexcept = 0;

    // Submit a semantic system action (volume up / volume down / show
    // desktop / power escape / open Codex / etc.). Resolved by the
    // implementation; see §3.5 ``ActionResolver``.
    virtual bool submit_system_action(SystemAction action) noexcept = 0;

    // Cancel all queued actions that have not yet been delivered to
    // the OS. Used at shutdown + at F5 release during voice transition.
    virtual void cancel_pending() noexcept = 0;

    // Start the sink (open Win32 SendInput handle). Idempotent.
    virtual bool start() noexcept = 0;

    // Stop the sink. Idempotent. Implies cancel_pending().
    virtual void stop() noexcept = 0;
};
```

- 不变量：回调路径**绝不**调用 `IHostActionSink::submit_*` —— 钩子只更新原子状态 + 入队；真正的 `submit_*` 由协调层（Phase 7 `Application`）在主线程上调用。这是 plan §3 rule 6 的实施细节。
- `submit_*` 返回 `bool` 而非抛异常 —— 输入/动作路径禁用异常。

#### 3.4 `remotemic::input::LowLevelKeyboardHook`

- Windows-only；安装 `WH_KEYBOARD_LL` 钩子。
- 钩子回调路径**只**做：
  1. 读取 `KBDLLHOOKSTRUCT` 的 `vkCode` / `scanCode` / `dwExtraInfo` / `flags` 字段
  2. 检查原子 `std::atomic<bool>` 抑制表（O(1) lookup）
  3. 通过已注册的 lock-free SPSC queue 把 `InputEvent` 入队（不超过 256 项；溢出时丢最旧 + 累加 `dropped_count_`）
  4. 返回 `LRESULT` —— `1`（吞掉）或调用 `CallNextHookEx`（转发）
- 钩子回调**绝不做**：`SendInput` / `GetMessage` / `WriteFile` / Frida IPC 读取 / Python GIL acquire / `printf` / 任何 `std::mutex` 等待。
- 钩子回调延迟预算：**5 us**（计时器在 hook 入口 / 出口采样，超阈值 → 计入 `slow_callback_count_`）。
- 不可重入：同一线程递归调用 hook 路径时返回 `CallNextHookEx`，不进入本 hook 业务逻辑。
- 关闭路径：`UnhookWindowsHookEx` + queue drain + `cancel_pending()` on the registered sink。

#### 3.5 `remotemic::input::ActionResolver`

- 纯逻辑，无平台依赖；接受 `SystemAction` 枚举值 + 可选的 user keymap（**只**接受**已解析**的 keymap，不读 JSON / YAML / 任何文件）。
- 解析规则（与 `key_mapping.py` 行为一致但简化）：
  - 内部硬编码的"默认动作表"（与 Phase 4 之前 Python baseline 一致：方向键 → 对应动作；电源 → Escape；菜单 → 上下文菜单；TV → 应用切换；主页 → 显示桌面；音量+/− → 系统音量+/−；返回 → delete_backward；确定 → Return；麦克风 → 专用语音生命周期，**不**经过本 resolver）
  - 用户 keymap 覆盖：传入 user binding 时优先用户动作；缺失则回落到默认表
  - 返回 `std::optional<ResolvedAction>`（VK 序列 / 系统调用标识）
- 不变量：用户 keymap **只**由协调层（Phase 7）注入；C++ 端不读 JSON、不解析 binding。Phase 5 closeout 之前协调层尚未实现，因此本步骤只接受 "no user keymap"（默认表行为）。
- 隔离约束：本模块**不**读进程名 / 窗口标题 / 已安装应用列表 —— core 状态机逻辑保持纯净（plan §3 rule 7）。

#### 3.6 `remotemic::input::FridaHidTapSource`（可选基础设施）

- Windows-only；通过 Frida IPC socket 读取上游 `remote-bridge-hub` 的 WUDFHost tap 报告。
- 数据流：socket → `recvfrom` → `InputEvent{SourceKind::FridaHidTap}` → 已注册的 sink 回调。
- Frida socket 阻塞读取在**独立 IO 线程**，不进入 hook 回调路径；socket 数据进入 lock-free SPSC queue 后再注入到 sink。
- Frida Gadget 不可用 / socket 不可读 / IPC 断连 → `start()` 返回 `false`；不抛、不静默吞异常。
- 与 `RawInputSource` 互斥：协调层负责保证同一时刻只有一个处于 `started == true`。

#### 3.7 `remotemic::input::SendInputActionSink`

- Windows-only；`start()` 校验 `user32.dll` 可用 + 当前进程不是 0 token integrity 升级目标；`stop()` 取消 pending actions + 关闭内部 queue。
- `submit_key()` 路径：push 到内部 bounded queue（drop-oldest，容量 = 256）+ 通知 worker thread；worker thread 通过单次 `SendInput` 批量提交（一次性 syscall，per `win32_input.py` 已确立的批合同）。
- `submit_system_action()` 路径：直接调用 `SendMessage(HWND_BROADCAST, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_VOLUME_UP))` 等 Win32 广播路径（音量 / 媒体键）；显示桌面通过 `ShellExecuteW(L"explorer.exe", ...)` 或 `keybd_event(VK_LWIN, VK_D)`。
- 不可恢复错误：worker thread 写入失败 / 进程 token 丢失 → 计入 `submit_error_count_` + log，但不抛。

### 4. 接口边界（与 Python baseline 对齐）

| Python baseline | C++ 替代 | 边界 |
|---|---|---|
| `raw_input_windows.RawInputListener.start()` | `remotemic::input::RawInputSource::start()` | IInputSource 接口；source_kind 区分 RIM_TYPEKEYBOARD / RIM_TYPEHID |
| `legacy_key_suppressor_windows.LowLevelKeyboardHook` | `remotemic::input::LowLevelKeyboardHook` | IInputSource 接口；source_kind = LowLevelHookKeyboard；vk_code/scan_code/extra_info/injected/extended 字段填充 |
| `frida_hid_tap_runtime.TapRuntimeReader.run()` | `remotemic::input::FridaHidTapSource` | IInputSource 接口；source_kind = FridaHidTap |
| `win32_input.send_key` / `send_key_combo` / `send_key_combo_up` | `remotemic::input::SendInputActionSink::submit_key` | IHostActionSink 接口；批合同在实现层保持 |
| `key_mapping.resolve` | `remotemic::input::ActionResolver::resolve` | 纯逻辑；接受已解析 user keymap（不读 JSON） |
| `hotkey.physicalize_voice_hotkey` | `remotemic::input::HotkeyPhysicalizer` | IHostActionSink 接口；`physicalize_voice_hotkey(L"RAlt")` 提交对应 VK 序列 |

### 5. 时间注入合同 + 回调时限

- `LowLevelKeyboardHook` 内部计时器（`QueryPerformanceCounter`）测量回调入口 / 出口延迟；超阈值（默认 5 us）计入 `slow_callback_count_`，并在 `stop()` 时 log。不阻塞回调路径。
- Frida socket 读取线程使用 `steady_clock` deadline，与 Phase 4 `WasapiAudioRoute::drain` 同款模式（不暴露 `ClockFn`，必须真实时钟，避免超时判断失真）。
- ActionResolver 接受可选的 `ClockFn now` 用于 future "动作速率限制"扩展；当前不消费。
- 注入位避免 Phase 6 / 7 重构时改签名。

### 6. Python/native 切换与回退

- 按 `ADR-0011` §3 沿用 `_remotemic_native_runtime.py` 三态；本阶段新增 module keys：
  - `raw_input_source` → `python`（默认）/ `native` / `shadow`（**仅测试用**；真实 Frida socket 不可在 shadow 中开两份）
  - `host_action_sink` → `python`（默认）/ `native` / `shadow`
  - `key_suppressor` → `python`（默认）/ `native` / `shadow`
- 默认实现 `python` 仍走 `legacy_key_suppressor_windows.LowLevelKeyboardHook` / `win32_input` / `raw_input_windows.RawInputListener`（保留为 fallback 直到 Phase 8 closeout 删 Python）。
- 切到 `native` 时由对应 C++ 实现替换；Python fallback 实例化为 `None` 占位。
- C++ binding smoke：每个新模块一份 Python 单测（与 Phase 2/3/4 同口径），覆盖：
  1. 默认实现仍是 `python`
  2. `input_event_native_constructs_pod`（值类型 round-trip）
  3. `i_input_source_native_routes_via_fake_endpoint_in_shadow_mode`（用 `FakeInputSource` 替代真实 hook）
  4. `i_host_action_sink_native_submits_via_fake_sink_in_shadow_mode`（用 `FakeActionSink` 替代真实 SendInput）
  5. `low_level_hook_native_drops_oldest_on_queue_overflow`（按事件计数验证）
- 真实 hook / Frida IPC 的 shadow **不**做（plan §3 rule 5）；通过 Python 端"假后端 + 真实 C++ IO 线程"在测试环境验证。
- Python 全量回归保持 `passed`（按 plan §8 终止条件）。

### 7. 不变量（持续验证）

- `LowLevelKeyboardHook` 回调路径**永不**调用 `SendInput` / `GetMessage` / `WriteFile` / Frida IPC 读取 / Python GIL acquire / `std::mutex` 等待；违反计入 `slow_callback_count_`。
- 钩子回调延迟 < 5 us（压力测试覆盖）；超阈值计入诊断但不阻塞。
- `FridaHidTapSource::` 与 `RawInputSource::` 互斥；同进程同时 `started == true` 的输入源数量 ≤ 1（协调层校验）。
- `ActionResolver::resolve` 不读进程名 / 窗口标题 / 已安装应用列表；core 状态机纯净。
- 用户 keymap **不**写入 C++ 端；C++ 端只接受已解析后的 binding map。
- 双 owner 旁路禁止（plan §3 rule 5）：Raw Input 与 Frida HID tap 不可同时声明同一按键。

### 8. C++ 实现目录与命名

- `include/remotemic/input/input_event.hpp`（新增）—— POD-ish 值类型 + 枚举
- `include/remotemic/input/i_input_source.hpp`（新增）—— 输入源接口
- `include/remotemic/input/i_host_action_sink.hpp`（新增）—— 动作接收器接口
- `include/remotemic/input/low_level_keyboard_hook.hpp`（新增）—— Windows-only 钩子
- `include/remotemic/input/raw_input_source.hpp`（新增）—— Windows-only Raw Input 适配
- `include/remotemic/input/frida_hid_tap_source.hpp`（新增）—— Windows-only Frida IPC 适配
- `include/remotemic/input/action_resolver.hpp`（新增）—— 纯逻辑动作解析器
- `include/remotemic/input/send_input_action_sink.hpp`（新增）—— Windows-only SendInput 接收器
- `include/remotemic/input/hotkey_physicalizer.hpp`（新增）—— voice hotkey 物理化
- `include/remotemic/input/fake_input_source.hpp`（新增）—— 跨 OS 测试用 recording double
- `include/remotemic/input/fake_host_action_sink.hpp`（新增）—— 跨 OS 测试用 recording double
- `src/input/input_event.cpp`（新增）—— 值类型构造 + 比较
- `src/input/low_level_keyboard_hook_stub.cpp`（新增）—— 钩子 stub（5 us 延迟为 ∞，返回默认）
- `src/input/raw_input_source_stub.cpp`（新增）—— Raw Input stub（不接 OS）
- `src/input/frida_hid_tap_source_stub.cpp`（新增）—— Frida tap stub（不接 IPC）
- `src/input/send_input_action_sink_stub.cpp`（新增）—— SendInput stub（不接 OS）
- `src/input/action_resolver_stub.cpp`（新增）—— resolver stub（返回 nullopt）
- `src/input/hotkey_physicalizer_stub.cpp`（新增）—— physicalizer stub（不投递）
- `src/input/fake_input_source.cpp`（新增）—— recording double 实现
- `src/input/fake_host_action_sink.cpp`（新增）—— recording double 实现
- 单元测试：
  - `tests/unit/test_input_event.cpp` —— 值类型 round-trip + 枚举 + 不变量
  - `tests/unit/test_i_input_source.cpp` —— fake source + callback 路径
  - `tests/unit/test_i_host_action_sink.cpp` —— fake sink + submit/cancel 路径
  - `tests/unit/test_low_level_keyboard_hook_stub.cpp` —— stub 红状态测试
  - `tests/unit/test_action_resolver_stub.cpp` —— stub 红状态测试
- 绑定扩展：`src/bind/bind_module.cpp` 新增 `m, "input"` 子模块暴露 `InputEvent` / `FakeInputSource` / `FakeHostActionSink` / `ActionResolver`；Python 包装在 `apps/windows/rc003/src/ovb_rc003/input_native.py`（新增）

### 9. 退出 / 中止门禁

- G1：6 模块 × Debug + Release CTest 全过
- G2：3 模块 pybind11 binding smoke 全过
- G3：3 模块 shadow parity 全过（其中 `raw_input_source` / `low_level_keyboard_hook` / `frida_hid_tap_source` 用 `FakeInputSource` 替代真实 OS hook，事件计数 / 抑制表 / 队列深度逐项与 Python baseline 对齐）
- G4：Python 全量回归保持 `passed`（含新增 `input_native.py` wrapper 的 0-fail 新增行）
- G5：boundary-scan 与 Phase 2/3/4 同口径（不暴露用户 keymap 内容到日志 / 不暴露 Frida socket 内容到日志）
- G6：真实 RC003 + Raw Input + LL hook + Frida tap 在 Notepad / Typeless（**仅 Typeless**）上各跑一次：方向 / OK / 菜单 / 主页 / TV / 电源 / 麦克风 + Back（若 Frida tap 到达）/ Vol+ / Vol-（若 Frida tap 到达）由用户实际观察；不是只跑自动化
- 任一影子漂移、任一 Python baseline 行为变化、任一 hook 回调超 5 us 阈值在压力测试下出现、任一 Frida IPC 与 Raw Input 同进程双 source 出现 → 立即停止，回退上一实现（plan §8）

### 10. 推荐提交粒度

按 plan §9 拆 3 个 commit：

1. **本 ADR + 接口扩展 + 黄金夹具 + TDD red-state 单元测试 stub**：`IInputSource` / `IHostActionSink` / `ActionResolver` 等新接口；7 个新模块的 `.hpp` + stub `.cpp`（stub 返回固定值，测试红状态）
2. **不接入生产路径的 C++ 实现（输入源 + 动作接收器）**：Raw Input / Frida HID tap / LL hook / SendInput 真实实现；单元测试转 green
3. **原生路径切换与回退验证**：`REMOTEMIC_NATIVE_CHOICE_*_INPUT*=native` + `FakeInputSource` 整条路径在 Python 侧跑通；默认仍 `python`

后续 closeout commit（与 Phase 3 / 4 closeout 同节奏）：
- CHANGELOG `[0.6.0-candidate]` + Phase 5 closeout；版本号按 Rule 2 在 closeout commit bump 到 `0.6.0-candidate`（CMakeLists.txt / Python `__version__` / `pyproject.toml` 三处 lock-step，installer AppVersion 按 Rule 1 不动）

## Consequences

- **正**：低级键盘钩子 + Raw Input + Frida IPC 三个独立输入源进入 C++ 后，钩子回调路径的"非阻塞 + 5 us 上限"有了独立单元测试覆盖（plan §3 rule 6 自动化验证）。
- **正**：`ActionResolver` 与 `key_mapping.py` 行为对齐但接受"已解析后的 binding"输入，避免 C++ 端读 JSON 的副作用；用户数据保留在 Python 端，符合 plan §3 rule 8。
- **正**：`IInputSource` / `IHostActionSink` 接口与 Phase 4 `IAudioRoute` 同构（同"原子状态 + 无阻塞入队 + bool 返回"），Phase 7 Application 协调层可以用统一模式驱动所有 C++ 基础设施。
- **正**：Qianwen 物理化逻辑通过 `HotkeyPhysicalizer` 隔离，本 ADR 不触及；保持 plan §3 rule 7 的核心状态机纯净。
- **风险**：低级键盘钩子在 elevation 不一致进程上被 UIPI 拒绝；与 `legacy_key_suppressor_windows.py` 当前在 Windows 11 上的行为一致，本 ADR 不修复（plan §3 rule 7 + 用户原话"先不管 bug"）。
- **风险**：Frida IPC socket 断连 / Frida Gadget 缺失 / WUDFHost 拒绝注入 — `FridaHidTapSource::start()` 必须 fail-closed；plan §8 立即回退到 python fallback。
- **风险**：Back / Volume+ / Volume- 的"Frida tap 到达率"未在 Phase 5 范围验证；G6 真实验收在 Phase 5 closeout 之前必须实际跑一次。若 G6 暴露"Frida tap 永远不到达"，按 plan §8 回退到当前 python baseline 并把 Back / Vol+ / Vol- 标记为 deferred（与 Phase 3 / 4 closeout 同口径）。

## Rejected alternatives

- **整体保留 `raw_input_windows.py` 不动**：被 plan §5 阶段 5 明确拒绝 —— 现有 Python 实现没有显式"钩子回调延迟上限 + 队列溢出计数"，无法独立验证 plan §3 rule 6（钩子回调非阻塞 + 5 us 阈值）。
- **把 Frida IPC 解析也迁入 C++**：被拒 —— Frida Gadget 是上游 `remote-bridge-hub` 的二进制闭源模块；本项目只读取 socket 数据流，不重写 IPC 协议。把 Frida 协议解析留 Python，本 ADR 只迁 socket reader + queue producer。
- **把 `key_mapping.py` 的 JSON 解析也迁入 C++**：被 plan §3 rule 8 明确拒绝 —— `key_bindings.json` 是用户数据；C++ 端不应读 JSON / YAML / 任何文件，只接受协调层已解析后的 binding map。
- **合并 Raw Input + Frida HID tap + LL hook 为单一"输入聚合器"**：被 plan §3 rule 5 明确拒绝 —— 三者互斥拥有按键生命周期；同一进程同时启用两份会破坏按键归属。
- **用 `WH_KEYBOARD` 高层 hook 替代 `WH_KEYBOARD_LL`**：被拒 —— 高层 hook 收到的是已注入的事件（LLKHF_INJECTED 标志），无法区分物理键与发送键；现有 `legacy_key_suppressor_windows.py` 使用 LL hook 是有意的。
- **直接在 C++ 端处理键位映射（写一份新 default keymap）**：被 plan §3 rule 8 拒绝 —— 用户的手动键位映射表是产品数据，本 ADR 不重写 default keymap，只接受"已解析后"输入。
- **shadow 模式覆盖真实 Frida socket / Real Raw Input**：被 plan §3 rule 5 拒绝 —— 双 owner 接收同一 IPC 数据流会破坏 tap 计数；shadow 只在 `FakeInputSource` + `FakeHostActionSink` 上跑。