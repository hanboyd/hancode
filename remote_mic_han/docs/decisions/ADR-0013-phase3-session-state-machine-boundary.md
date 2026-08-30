# ADR-0013: Phase 3 — VoiceController / ATVV 会话边界 / 释放消抖 / AUDIO_STOP 回退 / 关闭重试所有权的 C++ 迁移边界

- Status: **accepted**
  - This ADR records the **architecture decision** (state machine boundary, time-injection contract, single-session owner, Python/native fallback, 200 ms default / `[50ms, 500ms]` window, late-audio guard default 2500 ms, close-retry ownership, 6-step commit granularity). All of those are settled and accepted per the gate table in the `[0.4.0-candidate]` CHANGELOG entry (24/24 ctest Debug + 24/24 ctest Release).
  - **Real-device acceptance** is tracked separately, NOT in this ADR's status. It is `deferred` for:
    - RC003 真机端到端 (mic press → AudioStart → AudioStop → host hotkey)
    - Typeless 集成验收 (语音快捷键落入 Typeless 输入路径)
    - Qianwen 集成验收 (流式 PCM 经过 Session 边界后到达 Qianwen ASR)
  - The architecture decision being accepted does not contradict those three checks being deferred; they are different artefacts. The CHANGELOG records the deferred real-acceptance separately.
- Date: 2026-08-30
- Phase: 3 (of 9, per `docs/architecture/cpp-migration-execution-plan.md`)
- Related: [ADR-0003](ADR-0003-voice-edge-debounce-and-hook-decoupling.md)（语音边沿消抖与钩子解耦，已接受）、[ADR-0011](ADR-0011-cpp-python-binding-and-error-model.md)（Python/C++ 绑定与错误模型）、[ADR-0012](ADR-0012-atvv-adpcm-phase2-boundary.md)（Phase 2 ATVV/ADPCM 4 区）

## Context

Phase 1 / Phase 2 已交付：`remotemic_core` / `remotemic_native._C` / Python 包装层 / 模块级 `python`/`native`/`shadow` 三态切换、ATVV 能力解析、ATVV 控制编解码、IMA/DVI ADPCM 解码、ADPCM DC 高通 / 平滑增益 / `FrameAccumulator` 的 C++ 实现（4 区 × 6 门禁全过、Provenance 核验全过、`0.3.0-candidate` 已 bump）。C++ 已经能 byte/sample 级还原 Python 协议与 PCM 路径。

下一步按 `cpp-migration-execution-plan.md` §5 阶段 3，把**会话状态机和时序规则**迁入 C++ 纯核心。这一阶段的特殊性：

- 与 Phase 2 的纯函数 / 纯对象不同，**会话状态机持有跨多个 ATVV 控制事件的状态**（按下、释放、AUDIO_START、AUDIO_STOP、AUDIO_SYNC、断连、重连、关闭重试）。如果不在 C++ 端做单一 owner，Python 与 C++ 会各持一份"会话真相"，违反 plan §3 rule 5（"影子模式不得产生双重副作用，音频写入、按键注入和 BLE 写操作只能由一个实现拥有"）。
- 状态机的**时序行为**（200 ms 释放消抖窗口、2.5 s 稳定停止回退、关闭重试所有权）天然耦合到时间。如果不把时间注入，C++ 实现无法被单元测试直接验证，必须等真实硬件或真实 sleep。
- **VOICE_RELEASE_DEBOUNCE_SECONDS**（200 ms 默认，`[0.050, 0.500]` 限制）已经在 `config.py` 与 `voice_edge_debouncer.py` 双处落地。Phase 3 不重新发明，而是把这套契约搬到 C++，让两边都能跑同一份黄金夹具。
- ADR-0003 Fix A 已经把 LL hook 与应用锁解耦，Fix B / Fix C 的执行点位于 worker 线程。Phase 3 的状态机正是那个 worker 线程的消费对象：迁到 C++ 后，C++ 状态机接受来自 worker 的离散事件、返回 `VoiceHostAction` 命令、Python 仅做命令执行。
- 单 owner 规则的延伸：物理会话（physical session）至多对应一个 host session。每个 host session 至多一次开启 TAP、一次关闭 TAP（HOLD 模式下 KEY_DOWN / KEY_UP 必须配对）。

## Decision

### 1. Phase 3 范围（5 个子项）

| 子项 | Python 来源 | C++ 目标 | 类型 |
|---|---|---|---|
| `VoiceController` 状态机（TOGGLE / HOLD） | `voice_controller.py:44-148` | `remotemic::voice::VoiceController` + `remotemic::voice::VoiceHostAction` | 纯值类 |
| `VoiceEdgeDebouncer` 200 ms 释放消抖 | `voice_edge_debouncer.py:48-149` | `remotemic::voice::VoiceEdgeDebouncer` | 纯类（持 timer 引用，timer 可注入） |
| `ATVVSession` 状态机 + AUDIO_STOP 2.5 s 稳定停止回退 | `atvv_session.py:149-249` | `remotemic::atvv::Session` | 纯类（持时钟 + 内部解码器/滤波器/累加器引用） |
| 关闭重试所有权（host action 投递失败回滚） | `voice_controller.py:114-148` `restore_pending` / `cancel_pending` | `remotemic::voice::VoiceController` 的 `restore_pending` / `cancel_pending` | 纯类方法 |
| `voice_release_debounce_seconds` 配置语义 | `config.py:104-134` | C++ 配置加载期 `clamp_debounce_window`，Python 侧保持原 `config.py` 作为唯一加载入口，C++ 接受已经 clamp 过的值 | 纯函数 |

### 2. Phase 3 **非范围**（硬约束）

- BLE / WinRT 传输（`ble_transport_winrt.py` / `ble_discovery_windows.py`）—— Phase 6
- WASAPI / 音频设备路由（`audio_playback.py` / `audio_output.py` / `endpoint_*`）—— Phase 4
- Windows 输入采集 / Frida / Typeless / Qianwen / 键映射 / hotkey / Raw Input / LL hook —— Phase 5
- UI（`settings_ui.py` / `qt_settings_app.py` / `qml/`）—— 一直保留 PySide6，phase 9 才评估
- 打包 / 安装器 / 便携 ZIP / 签名发布 —— 按 `cpp-migration-version-policy.md` Rule 1 不做
- `connection_supervisor.py` 的物理按键事件聚合 —— 状态机迁完后**由 Python 侧继续持有**（它跨多种边界 input source，迁到 C++ 会把 Phase 5 的输入边界提前引入）
- 单连接多路复用（同一物理设备同时承担多 host session）—— 本期不做，留到 Phase 7 应用协调层

### 3. 模块合同

#### 3.1 `remotemic::voice::VoiceController`

- 构造：`VoiceController(VoiceTriggerMode mode, ClockFn now)`。`VoiceTriggerMode ∈ {TOGGLE, HOLD}`。`ClockFn` 是 `std::function<std::chrono::milliseconds()>`,默认 `monotonic_clock`；**所有时间判断**都通过它。
- `VoiceHostAction` 枚举：`TAP | KEY_DOWN | KEY_UP`（与 Python `voice_controller.VoiceHostAction` 逐字对应）。
- `VoiceHostAction on_mic_button_pressed() noexcept`：按下 → TOGGLE 返回 `TAP` 并把 `_toggle_active = true`；HOLD 返回 `KEY_DOWN` 并把 `_holding = true`。
- `std::optional<VoiceHostAction> on_audio_stopped() noexcept`：TOGGLE 若 `_toggle_active` 则返回 `TAP` 并清；HOLD 若 `_holding` 则返回 `KEY_UP` 并清；都未触发则返回 `nullopt`。
- `std::optional<VoiceHostAction> reset() noexcept`：强制关闭未结清 session，返回应做的收尾动作；调后 `active() == false`。
- `void restore_pending(VoiceHostAction) noexcept`：**仅当入参为 `KEY_UP` 或 `TAP` 时**恢复对应 pending 状态（与 Python `restore_pending` 行为一致，**不**接受其他值）。
- `void cancel_pending() noexcept`：清 pending 状态且不发任何动作（Python `cancel_pending` 同语义）。
- `bool holding() const noexcept` / `bool active() const noexcept`：查询接口，与 Python `holding` / `active` 一致。
- 不变量：TOGGLE 模式下 `holding()` 永远为 `false`；`active()` 仅在 `holding() || toggle_active()` 时为 `true`。

#### 3.2 `remotemic::voice::VoiceEdgeDebouncer`

- 构造：`VoiceEdgeDebouncer(std::chrono::milliseconds release_window, TimerFactory factory, ClockFn now)`。
  - `release_window` ∈ `[50ms, 500ms]`，越界抛 `std::invalid_argument`（与 Python `__init__` 的 `raise ValueError("release_window_seconds must be >= 0")` + `config.py` 的 clamp 双保险一致）。
  - `TimerFactory = std::function<std::unique_ptr<TimerHandle>(std::chrono::milliseconds, std::function<void()>)>` —— `TimerHandle` 暴露 `cancel()`。
  - 注入的 `ClockFn` 让测试用 `ManualClock`；production 用 `monotonic_clock`。
- `void on_press() noexcept`：取消任何 pending release；release_seq++。
- `void on_release(std::function<void()> handler) noexcept`：取消旧 pending，把 `(seq, handler)` 挂在 pending 上，调用 `factory` 创建 timer；测试中用 `ManualTimerFactory` 不真起线程。
- `void shutdown() noexcept`：取消 pending；cancellation 后的 in-flight firing 必须不再执行 handler（用 `seq` 单调递增做 stale 校验，与 Python `_release_seq` 一致）。
- `bool fire_pending_now_for_test() noexcept`：测试用同步触发接口，handler 只跑一次（与 Python `fire_pending_now_for_test` 行为一致）。
- `std::chrono::milliseconds release_window() const noexcept`：查询当前窗口值。
- 线程安全：与 Python 版一致，所有 `cancel` / `schedule` / `fire` 在内部 mutex 下；pybind11 暴露时使用 `py::call_guard<py::gil_scoped_release>` 让 worker 线程不阻塞 GIL。

#### 3.3 `remotemic::atvv::Session`

- 构造：`Session(double gain_db = 10.0, std::chrono::milliseconds late_audio_guard = std::chrono::milliseconds(2500), ClockFn now = monotonic_clock)`。
  - `late_audio_guard` 默认 2500 ms，对应 Python `proto.LATE_AUDIO_GUARD_SECONDS`；**可注入** 让测试用 0 / 50 ms 等小窗口。
  - `ClockFn` 给 `last_mic_off_at` 用；Python `time.monotonic` 语义直接对应 C++ `std::chrono::steady_clock`。
- 内部状态：`_caps`、`_version`、`_frame_size`、`_decoder`（Phase 2 的 `remotemic::adpcm::ImaDecoder`）、`_dc_filter`（Phase 2 的 `DcHighPassFilter`）、`_accumulator`（Phase 2 的 `FrameAccumulator`）、`_pending_sync`、`_mic_open`、`_last_mic_off_at`、`_last_session_id`。
- `ControlEvent handle_control(std::span<const std::uint8_t> payload)`：
  - 仍由 C++ 端先**纯字节解析** opcode（与 Phase 2 的 `remotemic::atvv::parse_control_message` 同源复用），再按状态机转移：CAPS → 设置 `_caps/_version/_frame_size` 并返回 `CapsReceived`；AUDIO_START → 复位 decoder/filter/accumulator、`_mic_open=true`、返回 `AudioStarted`；AUDIO_STOP → `_mic_open=false`、`_last_mic_off_at = now()`、`_accumulator.reset()`、返回 `AudioStopped`；AUDIO_SYNC → 记 `_pending_sync`、返回 `AudioSynced`；MIC_BUTTON → 直接返回 `MicButtonPressed`；其他 → `UnknownControl{opcode}`。
  - 控制事件类型用 **discriminated union** 或 `std::variant`；pybind11 暴露为简单 dataclass-like Python 类（与 Phase 2 的 `CapsReceived` 等 dataclass 形状一致）。
- `std::vector<std::int16_t> handle_audio(std::span<const std::uint8_t> payload)`：
  - 若 `!_mic_open && (now() - _last_mic_off_at) < late_audio_guard` → 返回 `{}`（late-audio 守卫）。
  - 否则 `_accumulator.append(payload, _frame_size)` → 对每个 emit 的 frame：若有 `_pending_sync` 则 `_decoder.reset(predictor, step_index)` + `_dc_filter.reset()`；`decoded = _decoder.decode(frame)`；`centered = _dc_filter.process(decoded)`；`pcm = remotemic::adpcm::postprocess(centered, gain_db)`；收集。
- `std::vector<std::uint8_t> mic_open_command() const`：返回 `remotemic::atvv::mic_open_command(_version)`。
- `std::vector<std::uint8_t> mic_close_command() const`：返回 `remotemic::atvv::mic_close_command(_version, _last_session_id.value_or(0))`。
- 公开查询：`const ATVVCapabilities* capabilities() const noexcept`、`bool mic_open() const noexcept`。

#### 3.4 关闭重试所有权

- C++ 状态机**不**做 host action 投递；投递由 Python worker 线程通过 `win32_input.send_key_combo_*` 完成。
- 投递失败时 worker 调用 `controller.restore_pending(action)`，把状态机回滚到"session 仍未结清"，下一次 worker tick 再重试；最多重试次数与超时由 Python 侧的 `_apply_voice_action` 保持现状（不引入新协议）。
- 投递成功时 worker 不调用 `restore_pending`，状态机保持"已结清"。
- 与 Python 当前 `restore_pending` / `cancel_pending` 行为完全一致；ADR-0003 Fix A/B/C 的所有现有 129 + 78 focused 测试不修改，全部由 C++ shadow 重新跑通。

#### 3.5 单会话 owner 规则

- C++ `Session` 一个实例对应**一个物理 BLE 连接**；同一物理连接不得在 Python 与 C++ 之间分裂状态。
- C++ `VoiceController` 一个实例对应**一个 host voice session**；TOGGLE 模式下同一时刻最多 1 个 pending 关闭 TAP；HOLD 模式下同一时刻最多 1 个 pending KEY_UP。
- Python `app.py` 的 `RC003App` 维持现状，**不**新增并行实例；现有 `_voice_trigger_lock` 不释放给 C++（C++ 不持应用级锁）。

### 4. 时间注入合同

- C++ 状态机**不**调用 `std::chrono::steady_clock::now()` 直接拿时间；所有读时间的入口都通过 `ClockFn now` 注入。
- production 默认实现：`auto monotonic_clock() -> std::chrono::milliseconds { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()); }`。
- 测试默认实现：`ManualClock { std::chrono::milliseconds value_; }`，提供 `advance(ms)` / `set(ms)` / `now()`。
- pybind11 不暴露 `ClockFn`（C++ 类型无法直接跨语言）；Python 侧继续用现有 `time.monotonic`，由 binding 层在构造时把 production `monotonic_clock` 装入 C++ 实例；测试在 C++ 侧直接调 `ManualClock` 注入，Python 不参与时间决策。

### 5. Python/native 切换与回退

- 按 `ADR-0011` §3 沿用 `_remotemic_native_runtime.py` 三态；本阶段新增 module key：
  - `voice_controller` → `python`（默认）/ `native` / `shadow`
  - `voice_edge_debouncer` → `python`（默认）/ `native` / `shadow`
  - `atvv_session` → `python`（默认）/ `native` / `shadow`
- `shadow` 模式仅当模块为 side-effect-free 时启用：3 个模块都是纯计算（输入事件序列 → 输出命令/PCM 列表），无 BLE 写、无 audio 写入、无 SendInput，**shadow 允许**。
- 默认实现 `python` 不变；切到 `native` 时由 C++ 实例替换，Python 实例化为 `None` 占位（与 Phase 2 的 `_DEFAULT_CHOICES["atvv_protocol"] = "python"` 同款）。
- C++ binding smoke：每个模块一份 Python 单测，覆盖（与 Phase 2 同口径）：
  1. 默认实现仍是 `python`
  2. `atvv_caps_native_returns_same_as_python` 类（事件级）
  3. `voice_controller_native_returns_same_actions_as_python`（按下/释放/重置/restore_pending 全事件）
  4. `voice_edge_debouncer_native_window_respects_injection`（ManualClock + ManualTimerFactory）
  5. 跨状态 `Session` + `VoiceController` 联动 shadow 一致性
- Python 全量回归保持 `passed`（按 plan §8 终止条件：baseline 行为不变）。

### 6. 不变量（持续验证）

- 物理会话 → host session：1 → 1，最多；TOGGLE 至多一对开 TAP / 关 TAP；HOLD 至多一对 KEY_DOWN / KEY_UP。
- 200 ms 默认值与 `[0.050, 0.500]` 范围不变，除非另立 ADR。
- `restore_pending` 仅接受 `KEY_UP` / `TAP`（其它值调用是 caller bug，行为同 Python 现状）。
- `_mic_open=false` 且 `(now - _last_mic_off_at) < late_audio_guard` → `handle_audio` 返回 `[]`。
- AUDIO_START 复位 decoder / dc_filter / accumulator，并清 `_pending_sync`。

### 7. C++ 实现目录与命名

- `include/remotemic/voice/voice_controller.hpp`
- `include/remotemic/voice/edge_debouncer.hpp`
- `include/remotemic/atvv/session.hpp`
- `src/voice/voice_controller.cpp`
- `src/voice/edge_debouncer.cpp`
- `src/atvv/session.cpp`
- 单元测试：`tests/unit/test_voice_controller.cpp` / `test_edge_debouncer.cpp` / `test_atvv_session.cpp`
- 绑定扩展：续在 `src/bind/bind_module.cpp` 的 `m, "voice"` 子模块和 `m, "atvv_session"` 下；Python 包装在 `apps/windows/rc003/src/ovb_rc003/voice_bridge.py` / `atvv_session_bridge.py`（新增）

### 8. 退出 / 中止门禁

- G1：3 模块 × Debug + Release CTest 全过
- G2：3 模块 pybind11 binding smoke 全过
- G3：3 模块 shadow parity 全过（黄金夹具 = Phase 2 既有夹具 + 新增事件序列夹具）
- G4：Python 全量回归保持 `passed`（含新增 bridge 包装器的 0-fail 新增行）
- G5：boundary-scan 与 Phase 2 同口径
- 任何 1 个影子漂移、任何 Python baseline 行为变化、任何 hook-thread 重新阻塞、任何 Python/C++ 双 session 真相出现 → 立即停止，回退上一实现（plan §8）

### 9. 推荐提交粒度

按 plan §9 拆 6 个 commit：

1. **合同、夹具、ADR**：本 ADR + C++ 头文件 + TDD red-state 单元测试（stub）
2. **不接入生产路径的 C++ 实现**：3 模块的 `.cpp` 实现，单元测试转 green
3. **绑定 / 适配与默认关闭的开关**：pybind11 暴露 + Python bridge + `_DEFAULT_CHOICES` 注册（默认 `python`）
4. **影子比对或假后端验证**：3 模块 shadow parity 全过
5. **原生路径切换与回退验证**：可选 `native` + 假后端 shadow
6. **文档、打包和验收证据**：CHANGELOG + Phase 3 closeout；版本号仍按 `cpp-migration-version-policy.md` Rule 2 在 closeout commit bump 到 `0.4.0-candidate`

## Consequences

- **正**：`VoiceController` / `VoiceEdgeDebouncer` / `ATVVSession` 三个状态机迁入 C++ 后，单一 owner 强制由 C++ 持有；Python 侧只发命令；worker 线程锁竞争显著降低（与 ADR-0003 Fix A 配套）。
- **正**：时间可注入后，所有跨时间的边界（200 ms 消抖、2.5 s 稳定停止、close-retry 重试上限）都可以由 C++ 单元测试在 0 ms / 100 ms / 500 ms 等离散点上无 sleep 验证。
- **正**：Phase 4 音频、Phase 5 输入、Phase 6 BLE 都可以基于这一层的 `Session` / `VoiceController` 上叠 C++ 实现，而不再有 Python/C++ 双 session 真相。
- **风险**：状态机跨多事件迁移时如出现行为差异（任何 TOGGLE/HOLD 路径下产生的 action 序列不同） → 立即停手，按 plan §8 回退，不解释用容差掩盖。
- **风险**：worker 线程与 C++ binding 之间的 GIL 处理如果失误会重新引入 ADR-0003 Fix A 解决的 hook-thread 阻塞；binding 层必须使用 `py::call_guard<py::gil_scoped_release>` 包裹所有可能触发 Python 回调的入口。
- **风险**：本阶段引入新 module key，Phase 1 baseline 的 `test_default_is_python` 等守卫测试在新 `_DEFAULT_CHOICES` 注册后必须仍以 `python` 为默认；按 Phase 2 已落地的 `_DEFAULT_CHOICES` 字典 literal 写法扩展即可。

## Rejected alternatives

- **把状态机迁移推迟到 Phase 7（应用协调层一起迁）**：被 plan §5 阶段 3 明确拒绝 —— 那样会让 audio / input / BLE 都先于状态机跨语言，违反 plan §3 rule 3（单模块替换）。
- **只迁 `VoiceController` 不迁 `VoiceEdgeDebouncer` / `Session`**：被拒 —— 时序行为是状态机的固有部分，不一起迁会留下跨语言时序漏洞（race + double session truth）。
- **保留 Python 状态机为 shadow 双跑、不替换**：被 plan §3 rule 5 拒绝 —— 双 owner 违反"音频写入、按键注入只能由一个实现拥有"。
- **把 `restore_pending` 改成任意 action 都接受**：被拒 —— 现状拒绝任意值是 caller-bug 早失败信号，泛化会掩盖未来的 session 不一致。
- **让 C++ Session 直接调用 WASAPI / LL hook / BLE 写**：被本 ADR §2 范围与 plan §1 rule 2（pure core 不依赖平台层）拒绝。
