# ADR-0014: Phase 4 — Bounded Audio Queue + WASAPI AudioRoute C++ 迁移边界

- Status: **proposed**
- Date: 2026-08-31
- Phase: 4 (of 9, per `docs/architecture/cpp-migration-execution-plan.md`)
- Related: [ADR-0011](ADR-0011-cpp-python-binding-and-error-model.md)（Python/C++ 绑定与错误模型）、[ADR-0012](ADR-0012-atvv-adpcm-phase2-boundary.md)（Phase 2 ATVV/ADPCM）、[ADR-0013](ADR-0013-phase3-session-state-machine-boundary.md)（Phase 3 会话状态机）

## Context

Phase 1/2/3 已交付：`remotemic_core` / `remotemic_native._C` / Python 包装层 / 模块级 `python`/`native`/`shadow` 三态切换、ATVV 协议 + ADPCM 4 区 + ATVV `Session` 状态机 + `VoiceController` + `VoiceEdgeDebouncer` 的 C++ 实现（24/24 ctest Debug + 24/24 ctest Release 通过，`0.4.0-candidate` 已 bump）。

下一步按 `cpp-migration-execution-plan.md` §5 阶段 4，把**有界音频队列 + WASAPI AudioRoute**迁入 C++ 纯核心 + Windows 基础设施层。这一阶段的特殊性：

- 现有 `apps/windows/rc003/src/ovb_rc003/audio_playback.py`（240 行）的 `EndpointPlaybackSink` 把"队列 + 分块 + 重采样 + 写设备 + 排空"全部揉在一个 Python 类里，用 `sounddevice.OutputStream` 做底层；BLE 回调线程与 Python GIL 间接耦合，没有显式有界队列，溢出路径依赖 `sounddevice` 自己的内部缓冲。
- C++ 端已有 `include/remotemic/interfaces/audio_route.hpp`（22 行，纯虚接口 `IAudioRoute { start / write / stop }`），但**没有任何实现**。Phase 4 给该接口填入真实 WASAPI 后端，并把"有界队列 + 20 ms 分块 + 静默补帧 + 排空"作为基础设施职责。
- 用户本轮明确表态（2026-08-31）：
  - "先不管 bug"——Typeless 集成验证里的"勉强能用"老 bug **不在 Phase 4 范围内**，不在本 ADR 决策面内，不为它改任何接口。
  - "保证基本功能可用的前提先完成重构"——Phase 4 必须保持现有 Typeless 路径仍然可用（即产品行为对外不变）。
  - "先不管千问，这个有点复杂"——`qianwen_physicalizer.py` 与 Qianwen 相关验证 / Frida 适配器**完全不在 Phase 4 范围内**，本 ADR 不触及。
- `cpp-migration-execution-plan.md` §3 rule 5 明确禁止音频写入"双 owner"。Python 端 `EndpointPlaybackSink` 与 C++ 端 `WasapiAudioRoute` 必须**互斥**地拥有 WASAPI 客户端句柄；`shadow` 双跑**不允许**。
- `cpp-migration-execution-plan.md` §3 rule 6 要求 BLE 回调、低级键盘钩子和音频回调中不得执行阻塞文件、进程、设备打开或日志刷新操作。Phase 4 把"分块 + 写入"放到独立写线程，BLE 回调只做无阻塞入队。
- `cpp-migration-version-policy.md` Rule 2：本阶段 closeout 时版本号 0.4.0-candidate → 0.5.0-candidate，三处（CMakeLists.txt / Python `__version__` / `pyproject.toml`）lock-step bump；installer `AppVersion` 按 Rule 1 不动。

## Decision

### 1. Phase 4 范围（4 个子项）

| 子项 | Python 来源 | C++ 目标 | 类型 |
|---|---|---|---|
| 有界 PCM 队列（drop-oldest，2 s 上限） | 当前**没有显式实现**（依赖 `sounddevice.OutputStream` 内部缓冲） | `remotemic::audio::BoundedPcmQueue<T>` | 模板纯类 |
| 20 ms 分块 + 静默补帧 + 排空策略 | `audio_playback.py:184-227` `drain()` + `close()` 的 pollling/timeout/稳定轮询语义；分块与静默补帧当前在 `sounddevice.write` 里隐式完成 | `remotemic::audio::PcmChunker` + `WasapiAudioRoute::run_writer_loop_` 内部 | 纯类 + 私有成员 |
| 16 kHz → 48 kHz 插值（line-preserving 升采样） | `audio_playback.py:154-172` 三阶线性插值 | `remotemic::audio::Upsample16kTo48k` | 纯函数 / 纯类 |
| `IAudioRoute` WASAPI 后端 | 现有 `include/remotemic/interfaces/audio_route.hpp`（仅接口） | `remotemic::audio::WasapiAudioRoute` + `FakeAudioRoute` | 实现接口 |

### 2. Phase 4 **非范围**（硬约束）

- BLE / WinRT 传输（`ble_transport_winrt.py`）—— Phase 6
- Windows 输入采集 / Frida / Typeless 集成 bug / Qianwen 适配器 / 键映射 / hotkey / Raw Input / LL hook —— Phase 5，且 Phase 5 也**不修** Typeless 老 bug
- UI（`settings_ui.py` / `qt_settings_app.py` / `qml/`）—— 一直保留 PySide6
- 打包 / 安装器 / 便携 ZIP / 签名发布 —— 按 `cpp-migration-version-policy.md` Rule 1 不做
- 端点枚举与选择（`audio_output.py` 的 `preferred_output_endpoints` / `resolve_selected_endpoint` / `enumerate_output_endpoints`）—— **留在 Python**，理由：它是表现层 + 选择层 + 隐私相关（端点字符串进日志），与 plan §3 rule 2"核心不依赖平台层"无冲突；sounddevice 仅 Windows 需要，已经 lazy import；C++ 端只接收 `(endpoint_name, host_api)` 字符串作为 `WasapiAudioRoute::start` 的入参，C++ 端自己 enumerate WASAPI 设备并按名字匹配。
- 双 owner 旁路（同时跑 Python sink + C++ WASAPI）—— plan §3 rule 5 禁止
- 虚拟麦克风驱动替换 / VB-CABLE 重打包 —— plan §2 不在本路线
- 关闭重试所有权（`VoiceController.restore_pending` / `cancel_pending`）—— Phase 3 已交付，本阶段不重新设计
- 16 kHz / 单声道 / 16-bit 合同变更 —— **保持不变**，除非另立 ADR

### 3. 模块合同

#### 3.1 `remotemic::audio::BoundedPcmQueue<T>`

- 模板类型 `T` 限定为 `std::int16_t`（mono PCM）。
- 构造：`BoundedPcmQueue(std::size_t capacity_samples, ClockFn now = monotonic_clock)`。
  - `capacity_samples` 默认 = `2 * SOURCE_SAMPLE_RATE_HZ`（2 秒 @ 16 kHz = 32 000 samples）。
  - 入参必须 > 0；越界抛 `std::invalid_argument`。
- `void push(std::span<const std::int16_t> samples) noexcept`：队满时**丢弃最旧**数据并把 `dropped_count_` 累加 `samples.size()`；新数据**始终入队**（drop-oldest 而非 drop-newest，与 Phase 4 plan §5 一致）。
- `std::vector<std::int16_t> pop_up_to(std::size_t max_samples) noexcept`：弹出至多 `max_samples` 个样本；空时返回空 vector。
- `std::size_t size() const noexcept`：当前队列样本数。
- `std::uint64_t dropped_count() const noexcept`：自构造以来累计丢弃样本数（永不归零，跨进程统计用）。
- 线程安全：所有 public 方法在内部 mutex 下；pybind11 暴露时使用 `py::call_guard<py::gil_scoped_release>` 让 producer 线程（BLE 回调 / Python worker）不阻塞 GIL。
- 不变量：`size() <= capacity_samples`；`dropped_count_` 单调递增。

#### 3.2 `remotemic::audio::PcmChunker`

- 构造：`PcmChunker(std::chrono::milliseconds chunk_duration, std::uint32_t sample_rate_hz = 16'000)`。
  - `chunk_duration` 默认 20 ms；对应每块 320 samples（@ 16 kHz）。
  - 越界 / 0 抛 `std::invalid_argument`。
- `std::optional<std::vector<std::int16_t>> next_chunk(std::span<const std::int16_t> incoming) noexcept`：把 `incoming` 追加到内部 buffer；若累积够 `chunk_duration` 则返回完整块并把残余保留；否则返回 `nullopt`。
- `std::vector<std::int16_t> flush_remaining_with_silence() noexcept`：返回 buffer 中残余 + 静默补到 `chunk_duration` 的最后一帧；调用后 buffer 清空。
- 线程安全：单线程使用；生产者与分块器在同一线程（写线程）。
- 静默补帧：`std::int16_t{0}` 填充；语义与"buffer underrun → 静音"等价，避免 WASAPI 端回放最后样本的拖尾。

#### 3.3 `remotemic::audio::Upsample16kTo48k`

- 纯函数：`std::vector<std::int16_t> upsample_16k_to_48k(std::span<const std::int16_t> source, std::int16_t previous_sample, bool have_previous)`。
- 与 `audio_playback.py:154-172` 三阶线性插值**逐字节对齐**：每个源样本展开为 `(prev + δ/3, prev + 2δ/3, current)`，四舍五入到最接近的 int16，`std::clamp(-32768, 32767)`。
- 不持有状态；调用方传入 `previous_sample` / `have_previous`，避免依赖全局。
- 单元测试：与 Python `audio_playback.py:154-172` 的 `_select_output_sample_rate` 返回 48 000 Hz 时产生的输出**逐样本一致**（黄金夹具来自现有 Python 测试）。

#### 3.4 `remotemic::audio::WasapiAudioRoute`（实现 `IAudioRoute`）

- 构造：`WasapiAudioRoute(std::wstring endpoint_name, std::wstring host_api_name = L"") noexcept`。
  - endpoint 名 = `audio_output.resolve_selected_endpoint` 返回的 `AudioEndpoint.name` / `host_api` UTF-16 化；C++ 端**自己**用 `WASAPI::IMMDeviceEnumerator::EnumAudioEndpoints` 按名字 + host API 匹配（host API 用 `"Windows WASAPI"` 子串匹配即可）。
- `bool start(PcmFormat format)`：
  - 通过 `WASAPI::CoCreateInstance(CLSID_MMDeviceEnumerator)` 获取 `IMMDeviceEnumerator` → `GetDevice(endpoint_name)` → `Activate(IAudioClient)`。
  - `IAudioClient::IsFormatSupported` 检查 16 kHz / 48 kHz；若设备首选 48 kHz，则 C++ 端用 `Upsample16kTo48k` 做升采样后再写（与现有 Python 路径行为一致）。
  - 选择 `AUDCLNT_SHAREMODE_SHARED` + 低延迟 (`AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM` 不开，避免自动转换吃源样本)。
  - `IAudioClient::Initialize` + `IAudioClient::Start`；写线程 `std::jthread` 启动，`run_writer_loop_` 进入循环。
  - 返回 `true` 表示 WASAPI 流已开启且写线程已起；返回 `false` 表示设备未找到 / 不支持 / 激活失败，调用方 fail-closed。
- `bool write(std::span<const std::int16_t> samples)`：
  - 无阻塞入队 `BoundedPcmQueue`；返回 `true` 表示已入队（含丢弃最旧的入队）；返回 `false` 表示路由已 stop / close。
- `void drain(std::chrono::milliseconds timeout)`：
  - 等待队列 size 降到 0 或超时；与 `audio_playback.py:184-227` 的 `drain()` 行为对齐（写可用缓冲稳定 3 个 poll / 60 ms）。
- `void stop() noexcept`：
  - 标记 `_stop_requested`；写线程在当前 chunk 写完后退出；`IAudioClient::Stop`；`IAudioClient::Release`。
- `void close() noexcept`：
  - `stop()` 后调 `IMMDevice::Release`；把端点句柄释放。
- 内部线程：`std::jthread _writer_thread`；函数 `run_writer_loop_()`：
  1. `chunk = _chunker.next_chunk(_queue.pop_up_to(chunk_size))`；
  2. 若 device 需要 48 kHz：`upsampled = Upsample16kTo48k::upsample(chunk, _prev, _have_prev)`；
  3. `_audio_client->Write(chunk.data(), chunk.size() * sizeof(int16_t))`；写失败时记录 `_write_error_count_++`，不抛（plan §3 rule 6 不允许回调路径抛异常）。
  4. 重复直到 `_stop_requested && _queue.empty()`。
- 单 owner：写线程拥有 `_audio_client` 引用；主线程（Python / Python-side worker）只能调 `start` / `write` / `drain` / `stop` / `close`，不得跨线程持有 `_audio_client`。
- 线程安全：`BoundedPcmQueue` 自带 mutex；`stop` / `close` 通过 `_stop_requested` atomic flag 通知写线程退出。

#### 3.5 `remotemic::audio::FakeAudioRoute`（测试用）

- 实现 `IAudioRoute`；把所有 `write()` 收到的 samples 追加到内部 `std::vector<std::int16_t> _recorded_samples_`；额外记录 `started_count_` / `stopped_count_` / `write_call_count_` / `dropped_count_`。
- `start(PcmFormat)` 返回 `true` 始终；`stop()` 无副作用。
- 不依赖 Windows；测试可以跑在任意 OS。

### 4. 接口扩展（`IAudioRoute`）

为支持排空语义，给 `include/remotemic/interfaces/audio_route.hpp` 加一个方法：

```cpp
class IAudioRoute {
public:
    virtual ~IAudioRoute() = default;
    virtual bool start(PcmFormat format) = 0;
    virtual bool write(std::span<const std::int16_t> samples) = 0;
    virtual void drain(std::chrono::milliseconds timeout) noexcept = 0;  // 新增
    virtual void stop() noexcept = 0;
    virtual void close() noexcept = 0;  // 原 stop 拆为 stop + close
};
```

原 `stop()` 语义保留；`close()` 是新增的"释放设备句柄"，语义等价 Python `EndpointPlaybackSink.close()`。Phase 3 已有的 `IAudioRoute` 使用者（本仓库内目前只有 Phase 4 引入）必须同步更新。**不**保留旧的"stop 即 close"双义 API。

### 5. 时间注入合同

- `BoundedPcmQueue` 持 `ClockFn now = monotonic_clock` 用于将来可能的"过期队列"扩展（**当前不消费**，但保留注入位以避免 Phase 5 / 6 / 7 重构时改签名）。
- `PcmChunker` 不持时间（纯逻辑）。
- `WasapiAudioRoute::drain` 用 `steady_clock` 内部，与 Python `audio_playback.py:208-227` 同样的"deadline = monotonic + timeout"模式；不暴露 `ClockFn`（drain 必须用真实时钟，避免超时判断失真）。

### 6. Python/native 切换与回退

- 按 `ADR-0011` §3 沿用 `_remotemic_native_runtime.py` 三态；本阶段新增 module key：
  - `audio_route` → `python`（默认）/ `native`（**`shadow` 禁止**，plan §3 rule 5）
- 默认实现 `python` 仍走 `EndpointPlaybackSink`（保留为 fallback 直到 Phase 8 closeout 删 Python）。
- 切到 `native` 时由 `WasapiAudioRoute` 替换；Python `EndpointPlaybackSink` 实例化为 `None` 占位。
- C++ binding smoke：每个新模块一份 Python 单测（与 Phase 2/3 同口径），覆盖：
  1. 默认实现仍是 `python`
  2. `bounded_queue_native_drops_oldest_on_overflow`（按样本计数验证）
  3. `chunker_native_emits_20ms_chunks_with_silence_padding`（最后一帧不完整时补零）
  4. `upsample_16k_to_48k_native_matches_python_byte_exact`（与 Python baseline shadow）
  5. `audio_route_native_routes_through_fake_endpoint_in_shadow_mode`（用 `FakeAudioRoute` 替代真实 WASAPI，验证整条路径）
- 真实 WASAPI 端点的 shadow **不**做（plan §3 rule 5）；通过 Python 端"假后端 + 真实 C++ 写线程"在测试环境验证。
- Python 全量回归保持 `passed`（按 plan §8 终止条件）。

### 7. 不变量（持续验证）

- `BoundedPcmQueue::size() <= capacity_samples` 恒成立。
- `dropped_count_` 单调递增；`stop()` / `close()` 不归零。
- WASAPI 流开启时只存在一个写线程；`stop()` 后写线程在 ≤ 1 个 chunk 周期内退出。
- 16 kHz / mono / int16 输入合同不变；48 kHz 输出仅在 `IsFormatSupported` 返回 `48 kHz` 时启用。
- 不引入"双 owner"：Python `EndpointPlaybackSink` 与 C++ `WasapiAudioRoute` 互斥存在；切换由 `_remotemic_native_runtime.choose_implementation` 在模块加载期一次性决定。
- 写线程永不调用 Python 回调 / 阻塞 I/O / 设备重打开（plan §3 rule 6）。

### 8. C++ 实现目录与命名

- `include/remotemic/audio/bounded_pcm_queue.hpp`（新增）
- `include/remotemic/audio/pcm_chunker.hpp`（新增）
- `include/remotemic/audio/upsample_16k_to_48k.hpp`（新增）
- `include/remotemic/audio/wasapi_audio_route.hpp`（新增）
- `include/remotemic/audio/fake_audio_route.hpp`（新增）
- `include/remotemic/interfaces/audio_route.hpp`（扩展 `drain` / `close`）
- `src/audio/bounded_pcm_queue.cpp`（新增）
- `src/audio/pcm_chunker.cpp`（新增）
- `src/audio/upsample_16k_to_48k.cpp`（新增）
- `src/audio/wasapi_audio_route.cpp`（新增）
- `src/audio/fake_audio_route.cpp`（新增）
- 单元测试：
  - `tests/unit/test_bounded_pcm_queue.cpp`
  - `tests/unit/test_pcm_chunker.cpp`
  - `tests/unit/test_upsample_16k_to_48k.cpp`
  - `tests/unit/test_fake_audio_route.cpp`
  - `tests/unit/test_wasapi_audio_route.cpp`（仅在 Windows CI 跑；用 `FakeAudioRoute` 在 Linux/macOS CI 替代）
- 绑定扩展：`src/bind/bind_module.cpp` 新增 `m, "audio"` 子模块暴露 `BoundedPcmQueue` / `WasapiAudioRoute` / `FakeAudioRoute`；Python 包装在 `apps/windows/rc003/src/ovb_rc003/audio_route_native.py`（新增）

### 9. 退出 / 中止门禁

- G1：4 模块 × Debug + Release CTest 全过
- G2：4 模块 pybind11 binding smoke 全过
- G3：4 模块 shadow parity 全过（其中 `audio_route` 用 `FakeAudioRoute` 替代真实 WASAPI，sample-count / peak / RMS / drop-count / drain-order 逐项与 Python baseline 对齐）
- G4：Python 全量回归保持 `passed`（含新增 `audio_route_native.py` wrapper 的 0-fail 新增行）
- G5：boundary-scan 与 Phase 2/3 同口径（不暴露端点名字到日志）
- G6：真实 RC003 + Typeless（**仅 Typeless**，Qianwen 不在本 ADR 范围） 的 16 kHz mono → 48 kHz 升采样 → 虚拟端点 → 听写器完整链路由用户实际观察；不是只跑自动化。
- 任一影子漂移、任一 Python baseline 行为变化、任一写线程阻塞 / 双 session 真相出现 → 立即停止，回退上一实现（plan §8）

### 10. 推荐提交粒度

按 plan §9 拆 6 个 commit：

1. **ADR-0014 + 接口扩展 + 黄金夹具 + TDD red-state 单元测试 stub**：`IAudioRoute` 加 `drain` / `close`；4 个新模块的 `.hpp` + stub `.cpp`（stub 返回固定值，测试红状态）
2. **不接入生产路径的 C++ 实现**：4 个 `.cpp` 真实实现；单元测试转 green
3. **绑定 / 适配与默认关闭的开关**：pybind11 暴露 + `audio_route_native.py` wrapper + `_DEFAULT_CHOICES["audio_route"] = "python"` 注册
4. **影子比对（`FakeAudioRoute` 替代真实 WASAPI）**：sample-count / peak / RMS / drop-count / drain-order 与 Python baseline 对齐
5. **原生路径切换与回退验证**：`REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` + `FakeAudioRoute` 整条路径在 Python 侧跑通；默认仍 `python`
6. **文档、打包和验收证据**：CHANGELOG `[0.5.0-candidate]` + Phase 4 closeout；版本号按 Rule 2 在 closeout commit bump 到 `0.5.0-candidate`（CMakeLists.txt / Python `__version__` / `pyproject.toml` 三处 lock-step，installer AppVersion 按 Rule 1 不动）

## Consequences

- **正**：音频队列有界 + 写线程独立后，BLE 回调永不阻塞（plan §3 rule 6 自动满足），即使音频端点丢失 / WASAPI 暂时不可用，队列只丢旧不丢新，UI 不卡。
- **正**：drop-oldest 策略让"队列满"成为可观测信号（`dropped_count_` 进入诊断），不掩盖 backpressure。
- **正**：16 kHz → 48 kHz 升采样对齐 Python baseline 后，虚拟端点的听写器不会因重采样切换产生音质差异；G6 真实验收的"格式、幅度、连续性"等价于 Phase 4 plan §5 的退出门禁。
- **正**：Phase 5（Windows 输入）和 Phase 7（应用协调层）都可以基于 C++ `IAudioRoute` 接口叠加新功能，不需触碰 Python sink。
- **风险**：WASAPI 在共享模式下被其他应用独占设备时 `Activate` 可能失败；start() 返回 false 的 fail-closed 路径必须在 Python 端日志清晰（不暴露设备名字给 UI / 上行），G5 boundary-scan 验证。
- **风险**：写线程与 BLE 回调同时入队时 `BoundedPcmQueue` 的 mutex 竞争可能引入额外延迟；要求 producer 端 `write()` 不持有任何 Python 锁（`py::call_guard<py::gil_scoped_release>` 包裹 binding 层）。
- **风险**：用户当前安装的 RC003 + Typeless 在 Python baseline 上"勉强能用"（用户原话）；切到 native 后若 WASAPI 后端有任何时序漂移，可能让 Typeless 出现新症状。Phase 4 closeout 不修 Typeless 老 bug，但若原生路径引入**新**症状，按 plan §8 立即回退到 `python` 实现，不掩盖。

## Rejected alternatives

- **保留 `sounddevice` 不动**：被 plan §5 阶段 4 明确拒绝 —— 现有 Python `EndpointPlaybackSink` 没有显式有界队列、没有 drop-oldest 计数、没有写线程模型，无法独立验证 plan §3 rule 6（回调不阻塞）。
- **把音频端点枚举也迁入 C++**：被拒 —— 端点枚举跨 WASAPI / DirectSound / MME / WDM-KS，与 sounddevice 强耦合；且端点字符串涉及隐私（plan §3 rule 6 / config.py FORBIDDEN_KEYS）。`audio_output.py` 留在 Python 作为 presentation + selection 层是干净的边界。
- **让 `audio_route` 支持 `shadow` 模式**：被 plan §3 rule 5 拒绝 —— 双 owner 写同一 WASAPI 端点会破坏音频。
- **用 `winrt::Windows::Media::Audio::AudioGraph` 替代 `IAudioClient`**：被拒 —— AudioGraph 是 WinRT 抽象层，与现有 `sounddevice.OutputStream` 行为差异过大；G6 真实验收需要保持产品行为对外不变。
- **保留旧 `IAudioRoute::stop` 含义（拆 stop + close）而新加 `close`**：被拒 —— 双义 API 会让 Phase 5 / 7 的代码混用两种语义。直接拆 stop + close 是干净的演进。
- **20 ms 分块放到 BLE 回调侧（Python / C++ AtvvSession）**：被拒 —— 那是 ATVV frame 边界（120 bytes / 7.5 ms），与音频设备帧（20 ms）解耦；混用会污染 Phase 3 的 `ATVVSession` 状态机。