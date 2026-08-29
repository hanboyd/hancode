# ADR-0012: Phase 2 — ATVV capability parse, control 编解码, IMA/DVI ADPCM, 格式与畸形输入的 C++ 迁移边界

- Status: accepted
- Date: 2026-08-29 (proposed) / 2026-08-30 (accepted with step 4 addenda)
- Phase: 2 (of 9, per `docs/architecture/cpp-migration-execution-plan.md`)

## Context

Phase 1（[ADR-0011](ADR-0011-cpp-python-binding-and-error-model.md)）已交付 `remotemic_core`/`remotemic_platform_win32`/`remotemic_native._C` 骨架和模块级 `python`/`native`/`shadow` 切换开关。下一步要把 **4 个**纯计算型 ATVV/ADPCM 模块从 Python（`apps/windows/rc003/src/ovb_rc003/atvv_protocol.py`、`atvv_session.py`）迁到 C++，但**严格不动**会话状态机、BLE 传输、音频设备、Windows 输入、Typeless/Qianwen 与 UI。

这是 roadmap §1 rule 1（"pure core must not depend on Python"）从理论走到落地的第一步：把这4个无副作用、无 I/O、无线程状态、无平台依赖的纯函数/纯对象先搬到 `remotemic_core` 同一族的新静态库里，再用 Phase 1 的 binding 暴露给 Python，让 `_remotemic_native_runtime` 的 `atvv_protocol` 模块键可以选 `python` / `native` / `shadow`。

Plan §1 rule 3 把"协议字节级、音频样本级匹配"列为不可妥协的硬约束；rule 4 要求每个迁过来的模块都有 `python`/`native`/`shadow` 切换；rule 8 给出中止条件（任何 byte/sample 漂移 = 立即回滚）。这两条共同决定了 **C++ 单元测试 + Python 基线 + 共享黄金夹具 + 运行时 shadow 校验** 这一组验证手段。

## Decision

### 1. Phase 2 范围（4 个区域）

| 区域 | Python 来源 | C++ 目标 | 类型 |
|---|---|---|---|
| ATVV capability parse | `atvv_protocol.py:65-101` `ATVVCapabilities.parse` | `remotemic::atvv::Capabilities::parse` | 纯函数 |
| ATVV control **encoding**（host → device） | `atvv_protocol.py:48-61` `mic_open_command` / `mic_close_command` | `remotemic::atvv::mic_open_command` / `mic_close_command` | 纯函数 |
| ATVV control **decoding**（device → host payload 解析） | **新增** `atvv_protocol.parse_control_message(payload)`；现有 `atvv_session.handle_control`（`atvv_session.py:178-223`）只保留状态机职责，改为消费新解析器的结果 | `remotemic::atvv::parse_control_message` | 纯函数（不持有状态） |
| IMA/DVI ADPCM 解码 + DC 高通 + 平滑增益 + FrameAccumulator | `atvv_protocol.py:104-228` `IMAADPCMDecoder` / `DCHighPassFilter` / `postprocess` / `FrameAccumulator` | `remotemic::adpcm::ImaDecoder` / `DcHighPassFilter` / `postprocess` / `FrameAccumulator` | 纯类（每个实例归调用方所有，不跨线程共享） |

### 2. Phase 2 **非范围**（同样硬约束）

- BLE / WinRT 传输（`ble_transport_winrt.py`）— 状态机入口属于 Phase 5/6
- WASAPI / 音频设备路由（`audio_playback.py` / `audio_output.py`）— 不动
- VoiceController / 会话状态机（`atvv_session.ATVVSession` 整体 / `voice_controller.py` / `connection_supervisor.py`）— **状态机逻辑保留 Python**；C++ 只暴露 **纯字段解析器** `parse_control_message`，session 自己拿结果决定怎么转
- Windows 输入 / Frida / Typeless / Qianwen / 键映射 / hotkey — 全部 out of scope
- UI（`settings_ui.py` / `qt_settings_app.py` / `qml/`）— out of scope
- 新打包 / 安装器 / 便携 ZIP — 按 `cpp-migration-version-policy.md` Rule 1 不做
- 版本号在 Phase 2 全部 4 区完成前保持 `0.2.0-candidate`；下一次小版本号 `0.3.0-candidate` 在 Phase 2 closeout commit 里一次性 bump

### 3. 模块合同

**`remotemic::atvv::Capabilities`** (`include/remotemic/atvv/capabilities.hpp`)
- 纯值类型：`version:u16`, `codecs:u8`, `interaction:u8`, `frame_size:u16`, `selected_codec:u8`, `sample_rate:double`。
- `static std::optional<Capabilities> parse(std::span<const std::uint8_t> data) noexcept`
- 输入长度 < 7 或 `data[0] != 0x0B` → `std::nullopt`（与 Python `None` 对齐，**不抛异常**）。
- 线程：const；可并发调用。
- 析构：trivial。

**`remotemic::atvv::ControlMessage`** (`include/remotemic/atvv/control.hpp`)
- `enum class Opcode : std::uint8_t { Caps, MicButton, AudioStart, AudioStop, AudioSync, Unknown }`
- `using ControlMessage = std::variant<CapsPayload, MicButtonPayload, AudioStartPayload, AudioStopPayload, AudioSyncPayload, UnknownPayload>`
- `parse_control_message(std::span<const std::uint8_t>) -> std::optional<ControlMessage>`：空 payload → `nullopt`；非空 → 永远返回一个解析结果，未知 opcode 包成 `UnknownPayload`。
- Encoding：
  - `std::vector<std::uint8_t> mic_open_command(std::uint16_t version)`
  - `std::vector<std::uint8_t> mic_close_command(std::uint16_t version, std::uint8_t session_id)`
  - 输出与 Python helper **逐字节相同**。
- 不持有任何会话/会话机状态；与 Python 的 `ATVVSession.handle_control` 解耦。

**`remotemic::adpcm`** (`include/remotemic/adpcm/{ima_decoder,dc_highpass,postprocess,frame_accumulator}.hpp`)
- `ImaDecoder`：无锁、单线程；`reset(predictor:i16, step_index:u8)`（clamp 到 `[-32768,32767]` / `[0,88]`），`decode(span<const u8>) -> std::vector<std::int16_t>`（每字节 2 样本，高 nibble 在前）；样本逐个等于 Python `IMAADPCMDecoder.decode`。
- `DcHighPassFilter`：状态由调用方拥有；`reset()`（清 `previous_input_` / `previous_output_` 并把 `initialized_` 翻回 `false`，下一个 `process()` 会重新用 `samples[0]` 初始化）、`process(span<const i16>) -> std::vector<std::int16_t>`。`reset()` 后的输出与一个全新构造的实例在相同输入上 sample-exact 相等（step 4 增加的 C++ 单元测试与 binding smoke 共同证明）。
- `postprocess(span<const i16>, gain_db:double) -> std::vector<std::int16_t>`：3-tap smoothing + 增益 clamp 到 `[-24,24]` dB + `NaN/inf` 当 0 dB 处理 + 输出 clamp 到 `[-32768,32767]`。
- `FrameAccumulator::append(span<const u8>, frame_size:u16) -> std::vector<std::vector<u8>>`、`reset() -> void`、`pending_size() -> size_t`。`frame_size == 0` → 返回空 list 且不写入 pending buffer（与 Python `FrameAccumulator.append` 的 `if frame_size <= 0: return []` 守卫逐字节对应；这就是为什么 C++ 端必须用 `std::uint16_t`：负值不可能到达 `append`，而 `0` 走显式 no-op 分支，不会被当作"缓冲区吃满"无限循环）。`reset()` 丢弃 pending（O(1)，保留 `capacity()`），调用后下一次 `append()` 与全新构造实例在相同输入/frame_size 下结果完全相同，**包括跨 `frame_size` 重分帧场景**（即"旧流遗留 partial 帧不会进入新流的 emitted 列表"——这是 step 4 要证明的不变量）。
- 线程：所有类型都非线程安全；所有权单一。

#### 3.1 `frame_size` 协议域（Area 4 step 4 corrective）

| 来源 | 范围 | 公共 API 行为 |
|---|---|---|
| 协议无效（no-op） | `frame_size <= 0`（包括 0 与所有负值） | `append()` 在 **Python/native binding 边界**被吸收为 no-op：`[]` 返回；**`pending_size` 不变**（既有 pending 与输入 `data` 都不被写入）。这与 Python 基线 `atvv_protocol.py:272-273` 的 `if frame_size <= 0: return []` 守卫逐字对应；不同点在于 binding 层额外保证 pending 不被改写，因为 C++ `append()` 本身的 `<= 0` 守卫已经保留 pending |
| 协议有效域 | `1..65535`（含两端） | 在 binding 层 narrow 到 `std::uint16_t` 后委托给 C++ core，分帧输出与 Python 基线 sample-equal |
| 协议无效（拒绝） | `frame_size > 65535` | 在 **Python/native binding 边界**显式抛 `TypeError`，不进入 C++ core，**`pending_size` 也不变**。这是 binding 层唯一一处显式异常上升点；类型为 `TypeError`，消息文本不被锁定（pybind11 升级不会带来测试假阳性） |
| C++ core 签名 | `std::uint16_t` | **未变**：`remotemic::adpcm::FrameAccumulator::append` 仍然接受 `std::uint16_t frame_size`；`<= 0` 在 C++ 端由 `if (frame_size <= 0) return out;` 守卫掉（防御性的，正常路径不会进入） |
| Invariant | `pending_size < frame_size <= 65535` | 每次成功 `append()`（`frame_size` 在 `1..65535`）后必然成立；`reset()` 后 trivial `pending_size == 0`。下界来自 `append()` 循环只 drain 整个 `frame_size` 字节，上界来自 `std::uint16_t` 类型

#### 3.2 Area 4 范围重申（step 4 补强）

Area 4 只承担：

1. **纯计算**：单极 IIR 高通 + 3-tap 平滑 + 直流移除非语音频段 → 由 `DcHighPassFilter` / `postprocess` 提供，无 I/O、无线程、无全局状态。
2. **字节重分帧**：把任意片段 ATVV 通知 payload 按 `frame_size` 切成定长帧 → 由 `FrameAccumulator` 提供；`reset()` 是"新流"边界，自己维护一个 `pending_` 缓冲。

Area 4 **不承担**：

- PCM 队列、20 ms 调度、WASAPI / 音频设备路由、线程 / 任务模型与会话状态机（这些属于 Phase 5/6 + `atvv_session` / `voice_controller` / `audio_playback`，不归此 ADR）。
- BLE / WinRT 传输，与 Area 1/2 同。
- 任何 release-build / frozen / PyInstaller 打包或安装器相关内容（见 §8 / 历史）。

Area 4 的 `reset()` 是 *协议级* "新流" 标记（一个新 20 ms PDU 流开始时由调用方决定调用），不是会话或线程同步原语。

### 4. 黄金夹具所有权（单一来源）

- 根目录：`apps/windows/rc003/tests/fixtures/atvv/`
- 现有：`synthetic-v1.json`（capabilities + ADPCM 最小夹具）
- Phase 2 新增（全部 synthetic，**不包含**任何捕获的真实设备/语音数据）：
  - `synthetic-v1-8k-fallback.json`
  - `synthetic-v1-zero-frame-size.json`
  - `synthetic-v1-zero-codecs-quirk.json`
  - `synthetic-legacy-pre-1.0.json`
  - `synthetic-legacy-rejects-short.json`（期望 C++ 返回 `nullopt`）
  - `synthetic-wrong-opcode.json`（期望 `nullopt`）
  - `synthetic-short-payload.json`（期望 `nullopt`）
- C++ 与 Python **共用同一 JSON 文件**。Python 由 `test_atvv_golden_fixture.py`（已有）+ 新增 capability 命名 fixture 测试读取；C++ 由 `tests/bind/atvv_fixture_loader.hpp` 用 `nlohmann::json`（FetchContent，pin 一个 release tag）解析。
- **不**允许出现第二份 hex / 期望值 copy-paste 进 `*.cpp`；C++ 测试如需 hex，必须从 JSON 读。
- 不引入真实设备抓取数据；Phase 2 全部 synthetic；真实抓取留给 Phase 9。

### 5. 验证矩阵（Python ↔ C++ 字节/样本严格相等）

详细表格见 Phase 2 plan report（本 ADR 同期的诊断说明）。硬约束：
- 控制消息产物：`bytes == bytes`（逐字节）
- PCM 样本：`list[i] == list[i]`（无容差，sample-exact）
- 任何 1 个差异 = 立即中止 Phase 2 该区域，回滚该区域全部 commit。

### 6. 运行时切换

`_remotemic_native_runtime.py` 注册模块名 `atvv_protocol`，env var `REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL` ∈ `{python, native, shadow}`：
- 默认 `python`（无产品行为变化）
- `native`：调用 `remotemic_native._C.atvv_capabilities_parse` 等绑定
- `shadow`：同时跑 python 与 native，逐字节/逐样本比对，不匹配抛 `RuntimeError`
- 4 区全部 pure compute → 全部允许 shadow

新增 Python 包装（`apps/windows/rc003/src/ovb_rc003/atvv_protocol.py` 内或同目录新 helper）提供与 Python 现有 helper 同名的入口（`parse_capabilities`、`encode_mic_open`、`encode_mic_close`、`decode_control`、`decode_adpcm_frame`、`apply_dc_highpass`、`postprocess_pcm`、`accumulate_frames`），内部按 env 切换。

### 7. CMake 结构

- 新静态库 `remotemic_atvv`（pure logic，无 `<windows.h>` / `<fstream>` / env-var / fs）
- 链接到 `remotemic_native_c`（pybind11 模块）与 C++ 单元测试目标
- 新可执行 `remotemic_atvv_tests`，注册 CTest：`remotemic_atvv_tests`
- 新 CTest：`remotemic_atvv_bind_smoke`（绑定烟雾，与 Phase 1 的 `remotemic_bind_smoke` 同形）
- 新依赖：`nlohmann::json`（FetchContent，pin `v3.11.3`），仅 fixture loader 使用
- 不动 `remotemic_platform_win32`、不改 `OUTPUT_NAME` / `RUNTIME_OUTPUT_DIRECTORY` / POST_BUILD `__init__.py` 复制逻辑

### 8. 迁移粒度（每区 6 步）

每个区域都按同一 6-step 拆分，单独 commit（per `cpp-migration-execution-plan.md` §9）：

1. **合同 + 夹具**（ADR 修订 + JSON fixture）
2. **C++ 头 + C++ 单元测试**（TDD：测试先写、对同一 JSON fixture、跑 CTest 失败）
3. **C++ 实现**（让 CTest 通过）
4. **pybind11 绑定 + binding smoke CTest**
5. **运行时 shadow 切换 + `test_atvv_native_parity.py`**
6. **区域 closeout**（ADR 状态 proposed→accepted、CHANGELOG 条目、INDEX 指向；**不**升版本号）

Phase 2 全部 4 区完成 → 单独一次 Phase 2 closeout commit，版本号 `0.2.0-candidate → 0.3.0-candidate`，CHANGELOG 收尾。

## Consequences

### 正向
- ATVV/ADPCM 这种**纯计算、字节敏感**的代码离开 Python 后，未来任何 Python 性能优化/类型注解改动都不会破坏协议兼容；固件级测试和 real-device 验收可以用 C++ 单元测试替代一半。
- `python`/`native`/`shadow` 切换在 phase 2 落地后，迁 BLE 状态机（phase 5）时已经具备运行时平行比对基础设施。
- 黄金夹具 JSON 化后，未来跨语言重构（包括把 C++ 改成 Rust、Go、或新版本 CPython）只需要重新实现 fixture loader，矩阵不变。

### 风险 / 约束
- 任何 byte-exact / sample-exact 不匹配 → 立即回滚该区域全部 commit（plan §8）。不允许宽松容差。
- `nlohmann::json` 是 Phase 2 唯一新增 C++ 依赖；必须 FetchContent + pin tag，pin 后不允许"用 main 分支"。
- 夹具 JSON 100% synthetic；任何尝试把真实抓取数据塞进 fixture 文件夹的提交都会被拒绝。
- `atvv_session.ATVVSession.handle_control` 改动严格限定为：把内联 opcode 分发改成调用新的 `parse_control_message`，其它任何业务逻辑（capability gate、decoder reset、sync predictor hold、late-audio guard）保持不变。

## Validation gates（要全部跑通才能把 ADR 状态从 `proposed` 改为 `accepted`）

G1. `cmake --build build/python --config Debug --target remotemic_atvv_tests remotemic_adpcm_ima_tests remotemic_adpcm_dc_tests remotemic_adpcm_postprocess_tests remotemic_adpcm_frame_tests --parallel && ctest --test-dir build/python -C Debug -R '^(remotemic_atvv_tests|remotemic_atvv_control_tests|remotemic_adpcm_ima_tests|remotemic_adpcm_dc_tests|remotemic_adpcm_postprocess_tests|remotemic_adpcm_frame_tests)$'` 全部 6 个 area 测试通过。
G2. 同样 `--config Release` 6 个 area 测试通过。
G3. `ctest --test-dir build/python -C Release -R '^(remotemic_atvv_bind_smoke|remotemic_atvv_control_bind_smoke|remotemic_adpcm_ima_bind_smoke|remotemic_adpcm_dc_bind_smoke|remotemic_adpcm_postprocess_bind_smoke|remotemic_adpcm_frame_bind_smoke)$'` 6 个 binding smoke 全部通过。
G4. `pytest tests/test_atvv_protocol.py tests/test_atvv_golden_fixture.py -q`（默认 python 模式）100% 通过。
G5. **运行时 shadow 等价证明（4 区分组）**。每个区都提供独立的 parity pytest / unittest，可单独 shadow：

  - Area 1：`REMOTEMIC_NATIVE_CHOICE_ATVV_PROTOCOL=shadow python -m unittest -p test_atvv_native_parity.py`
  - Area 2：`REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_PARSE=shadow REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_ENCODE=shadow python -m unittest -p test_atvv_native_parity_control.py`
  - Area 3：`REMOTEMIC_NATIVE_CHOICE_ADPCM_IMA_DECODE=shadow python -m unittest -p test_atvv_native_parity_adpcm.py`
  - Area 4：`REMOTEMIC_NATIVE_CHOICE_ADPCM_DC_HIGHPASS=shadow REMOTEMIC_NATIVE_CHOICE_ADPCM_POSTPROCESS=shadow REMOTEMIC_NATIVE_CHOICE_ADPCM_FRAME_ACCUMULATOR=shadow python -m unittest -p test_atvv_native_parity_area4.py`

  100% 通过（无容差；mismatch 即停，不放宽）。Area 4 step 5（commit `73e9155`）额外覆盖 `reset` 后状态、partial frame 跨调用、以及 `frame_size <= 0` 与 `> 65535` 的公共 API 行为；详见 `apps/windows/rc003/tests/test_atvv_native_parity_area4.py` 头注释。G5 通过前不视 Phase 2 area closeout 完成。

> **Phase 1 historical gate (G6) is removed.** Phase 1 曾经把"PyInstaller frozen `RemoteMicRC003.exe --dry-run` 在 bundled / stripped 两种状态下都通过"列为 Gate 3。该 gate 关心的是 frozen 打包产物，与 Phase 2 纯计算 4 区的 byte-exact / sample-exact 验收**没有直接因果关系**；继续保留它会让 Phase 2 closeout 卡在没有 fixed-version-phase-2 的 frozen 产物上，违背 `cpp-migration-version-policy.md` Rule 1（"在 Phase 2 完成前不做任何 frozen / 安装器 / 便携 ZIP 打包"）。Frozen 打包单独另开 ADR（计划列入 Phase 9 之后），不挂在此处。

## Non-goals（明确不做）

- 不实现 ATVV 的 encode（host → device）侧的版本协商；只迁 Python 已有的两个 helper。
- 不实现 IMA ADPCM 的 encoder；只迁 decoder（Python 也没有 encoder；`_reference_ima_encode` 是测试辅助，未来由 C++ 单元测试自己写一份同等效力的独立 encoder，作为 round-trip 测试对端）。
- 不优化、不 SIMD、不 NEON；先 byte-exact，再性能。
- 不改 Python 基线 `atvv_protocol.py` / `atvv_session.py` 的对外 API 形状；只新增 `parse_control_message` 一条 helper（不删旧 helper，保持兼容到 Phase 2 全部完成 + 单独一次清理）。

## References

- [ADR-0011](ADR-0011-cpp-python-binding-and-error-model.md) — binding tech + 错误/生命周期模型
- [`docs/architecture/cpp-migration-execution-plan.md`](../architecture/cpp-migration-execution-plan.md) §1（硬规则）/ §6（标准工作单）/ §8（中止条件）/ §9（提交粒度）
- `apps/windows/rc003/src/ovb_rc003/atvv_protocol.py` — Python 基线
- `apps/windows/rc003/src/ovb_rc003/atvv_session.py` — 状态机（只读，新增 parse_control_message 调用点）
- `apps/windows/rc003/tests/test_atvv_protocol.py` — Python 单元测试
- `apps/windows/rc003/tests/test_atvv_golden_fixture.py` — Python 黄金夹具测试
- `apps/windows/rc003/tests/fixtures/atvv/synthetic-v1.json` — 黄金夹具（现有）