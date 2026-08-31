# Changelog — Remote Mic RC003 (Windows)

## [Unreleased]

### Phase 5 / Phase 6（C++ 迁移）

（待开始。范围：Phase 5 Windows 输入、Phase 6 BLE。
Phase 4 已在 `0.5.0-candidate` 完成自动化门禁；真机 / Typeless /
Qianwen 验收 deferred；本节保留作下一阶段入口。）

---

## [0.5.0-candidate] — 2026-08-31 — Phase 4 implementation complete (automated gates passed; real RC003 / Typeless acceptance deferred per ADR-0014 §10)

**阶段状态：implementation complete；automated gates passed；real
RC003 / Typeless acceptance deferred。** Phase 4 不视为
完全关闭，直至真机 + Typeless 至少各跑过一次并观察（Qianwen
Frida adapter work 明确不在 Phase 4 范围内）。

**版本号说明**：按 `memory/cpp-migration-version-policy.md` Rule 2
预先 bump `0.4.0-candidate → 0.5.0-candidate`，同步
`CMakeLists.txt` 的 `project(VERSION ...)`、
`apps/windows/rc003/src/ovb_rc003/__init__.py` 的 `__version__`、
`apps/windows/rc003/pyproject.toml` 的 `version`、
`tests/bind/test_bind_smoke.py` 的 `info.version`。
`installer/RemoteMicRC003Setup.iss` 的 `AppVersion` 故意不动
（per Rule 1 — packaging 留给 phase 8）。即使真机验收 deferred，
版本号不回退，因为它跟踪的是 implementation+test 状态；后续若
真机验收发现 regression，按既定策略处理：先修复 + 复测，再按
`memory/cpp-migration-version-policy.md` Rule 1/2 决定是否需要
patch bump，而不是预先设定。

**范围**：9 阶段路线图第 4 阶段（C++/CPython 迁移第 6 区：
有界 PCM 队列 + 20 ms 分块 + 16 kHz → 48 kHz 升采样 +
WASAPI `IAudioRoute` 后端）。

**状态对象 shipping 路径已接入工厂**（产品路径现在通过
`make_audio_route()` 工厂 dispatch，对应
`app.py` 的 `self._playback` 构造点（`RC003App._open_playback_for_new_session`）；
默认 `python`，所以普通用户的体验与 Phase 3 完全一致；
设置 `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` 时真实产品路径
得到 `_NativeAudioRoute` 桥接包装（单 owner；`shadow` 禁止，
per plan §3 rule 5 — WASAPI 是 side-effecting）。
Phase 4 真实路由改动的门禁见下表 G5 行）。

详见 [ADR-0014](../../docs/decisions/ADR-0014-phase4-audio-wasapi-cpp.md)
第 1-10 节 + Phase 4 step 1-6 commits。

### Phase 4 / Area — BoundedPcmQueue + 20 ms PcmChunker + Upsample16kTo48k + WASAPI IAudioRoute 后端（C++ 迁移）

完成 Phase 4（ADR-0014）：把 Python 基线
`ovb_rc003.audio_playback.EndpointPlaybackSink` 的
"队列 + 分块 + 重采样 + 写设备 + 排空"职责迁移到 C++：

- `remotemic::audio::BoundedPcmQueue<T>` —
  mutex 保护的 drop-oldest 队列，构造期 `capacity_samples`
  默认 = `2 * SOURCE_SAMPLE_RATE_HZ`（2 秒 @ 16 kHz = 32 000）；
  满时丢最旧、`dropped_count_` 单调累加；不变量
  `size() <= capacity_samples`。pybind11 binding 层
  `py::call_guard<py::gil_scoped_release>` 释放 GIL。
- `remotemic::audio::PcmChunker` —
  20 ms 默认分块（@ 16 kHz = 320 samples），最后一帧
  不够时 `flush_remaining_with_silence()` 用 `int16_t{0}`
  补齐（与 Python `audio_playback.py` 的 `drain()` 静音
  补帧对齐）。
- `remotemic::audio::Upsample16kTo48k` —
  纯函数 `upsample_16k_to_48k(span, previous, have_previous)`；
  与 `audio_playback.py:154-172` 三阶线性插值**逐字节对齐**
  （每源样本展开 `(prev + δ/3, prev + 2δ/3, current)`，
  四舍五入 + clamp 到 `[-32768, 32767]`）。
- `remotemic::audio::WasapiAudioRoute` —
  Windows-only WASAPI 后端；`IAudioClient::IsFormatSupported`
  检查 16/48 kHz；若设备首选 48 kHz，C++ 端自动升采样；
  `AUDCLNT_SHAREMODE_SHARED` + 低延迟 + `AUTOCONVERTPCM` 关闭；
  独立写线程 `std::jthread` 串 `pop_up_to → next_chunk →
  upsample → IAudioClient::Write`；`stop()` 写完当前 chunk 后
  退出；`close()` 释放设备句柄。`start()` 返回 false 表示
  fail-closed。
- `remotemic::audio::FakeAudioRoute` — 记录 `recorded_samples_` +
  5 个 lifecycle counter（`started_count_` / `stopped_count_` /
  `write_call_count_` / `dropped_count_` / `write_error_count_`）；
  跨 OS 测试使用，**不**依赖 Windows。
- `IAudioRoute` 接口扩展 — 新增 `drain(timeout)` + `close()`；
  原 `stop()` 严格语义保留为"tell writer to exit"，**不**保留
  双义 API。Phase 4 引入的全部 `IAudioRoute` 实现必须同步
  更新签名。

Python ↔ C++ 字节级一致（无容差）：

- 8 个 Upsample16kTo48k 脚本（empty / single-no-previous /
  single-with-previous / multi-with-carry / negative / int16
  saturation / determinism / silence）
- 10 个 FakeAudioRoute vs FakePlaybackSink 脚本（single write /
  multiple writes / 20 ms chunk cadence / silence burst / alternating
  sizes / write-before-start dropped / write-after-close dropped /
  stop-idempotent / close-idempotent / loud signal）
- 6 个 lifecycle counter / peak / RMS / drop-count / sample-count
  / drain-order 对齐（6 dp）

门禁（ADR-0014 §9，全部 5 个 area × Debug + Release + binding +
shadow parity + native switch + production routing 同时满足）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 (C++) | `ctest -C Debug   -R '^(remotemic_bounded_pcm_queue_tests\|remotemic_pcm_chunker_tests\|remotemic_upsample_tests\|remotemic_fake_audio_route_tests\|remotemic_wasapi_audio_route_tests)\$'` | 5/5 通过 |
| G1 (C++) | `ctest -C Release -R '^(remotemic_bounded_pcm_queue_tests\|remotemic_pcm_chunker_tests\|remotemic_upsample_tests\|remotemic_fake_audio_route_tests\|remotemic_wasapi_audio_route_tests)\$'` | 5/5 通过 |
| G2 (Phase 2 + 3 全部不退化) | `ctest -C Debug   -R '^(remotemic_unit_tests\|remotemic_atvv_tests\|remotemic_atvv_control_tests\|remotemic_adpcm_ima_tests\|remotemic_adpcm_dc_tests\|remotemic_adpcm_postprocess_tests\|remotemic_adpcm_frame_tests\|remotemic_voice_controller_tests\|remotemic_edge_debouncer_tests\|remotemic_session_tests)\$'` | 10/10 通过 |
| G2 (Phase 2 + 3 全部不退化) | `ctest -C Release -R '^(remotemic_unit_tests\|remotemic_atvv_tests\|remotemic_atvv_control_tests\|remotemic_adpcm_ima_tests\|remotemic_adpcm_dc_tests\|remotemic_adpcm_postprocess_tests\|remotemic_adpcm_frame_tests\|remotemic_voice_controller_tests\|remotemic_edge_debouncer_tests\|remotemic_session_tests)\$'` | 10/10 通过 |
| G3 (binding smoke) | `ctest -C Debug   -R '^remotemic_audio_route_bind_smoke\$'` | 1/1 通过（10/10 子测试） |
| G3 (binding smoke) | `ctest -C Release -R '^remotemic_audio_route_bind_smoke\$'` | 1/1 通过 |
| G3 (byte-exact parity) | `ctest -C Debug   -R '^(remotemic_upsample_parity\|remotemic_audio_route_parity)\$'` | 2/2 通过（8/8 + 10/10） |
| G3 (byte-exact parity) | `ctest -C Release -R '^(remotemic_upsample_parity\|remotemic_audio_route_parity)\$'` | 2/2 通过 |
| G3 (version sync) | `ctest -C Debug   -R '^remotemic_bind_smoke\$'` | 1/1 通过（`info.version == "0.5.0"`） |
| G3 (version sync) | `ctest -C Release -R '^remotemic_bind_smoke\$'` | 1/1 通过 |
| G5 (shadow parity helper) | `python tools/verify_phase4_audio_parity.py` | 2/2 通过 |
| G5 (native switch + fake backend) | `ctest -C Debug   -R '^remotemic_phase4_native_switch\$'` | 1/1 通过（9/9；1 skipped：本地无 `_C.pyd`） |
| G5 (native switch + fake backend) | `ctest -C Release -R '^remotemic_phase4_native_switch\$'` | 同上 |
| G5 (production routing closeout) | `python tools/verify_phase4_native_switch.py` | 4/4 条件全部 PASS（默认 = python / 单 env → native shim / 恢复 → python / 无双 owner） |
| G5 (production routing closeout) | `ctest -C Debug   -R '^remotemic_phase4_production_routing\$'` | 4/4 通过 |
| G7 (Phase 3 production routing regression) | `ctest -C Debug   -R '^remotemic_phase3_production_routing\$'` | 17/17 通过（audio_route entry 不影响 voice/edge_debouncer/atvv_session） |
| G7 (Phase 3 production routing regression) | `python tools/verify_phase3_production_routing.py` | 19/19 PASS |
| 总计 | `ctest -C Debug` | 35/35 通过 |
| 总计 | `ctest -C Release` | 35/35 通过 |

新增内容：

- `include/remotemic/audio/bounded_pcm_queue.hpp` / `src/audio/bounded_pcm_queue.cpp`
  — `remotemic::audio::BoundedPcmQueue<T>` drop-oldest 模板（`T`
  限定 `std::int16_t`）；构造期 `capacity_samples` 默认 = 2 秒
  @ 16 kHz，越界抛 `std::invalid_argument`；所有 public 方法在
  mutex 下。
- `include/remotemic/audio/pcm_chunker.hpp` / `src/audio/pcm_chunker.cpp`
  — `remotemic::audio::PcmChunker` 20 ms 默认分块；`next_chunk`
  追加累积，够 `chunk_duration` 返回完整块，残余保留；
  `flush_remaining_with_silence()` 用 `int16_t{0}` 补到下一个
  `chunk_duration` 边界。
- `include/remotemic/audio/upsample_16k_to_48k.hpp` /
  `src/audio/upsample_16k_to_48k.cpp` — 三阶线性插值纯函数；
  与 `audio_playback.py:154-172` 字节级对齐。
- `include/remotemic/audio/wasapi_audio_route.hpp` /
  `src/audio/wasapi_audio_route.cpp` — WASAPI 后端；
  `IAudioClient::IsFormatSupported` 检测 + 48 kHz 自动升采样 +
  独立写线程 + atomic stop flag + drop-oldest 队列 + jthread
  writer pulling via PcmChunker+Upsample。
- `include/remotemic/audio/fake_audio_route.hpp` /
  `src/audio/fake_audio_route.cpp` — 跨 OS 测试 recording double
  (`FakeAudioRoute`)，记录 samples + 5 个 lifecycle counter +
  peak + RMS。
- `include/remotemic/interfaces/audio_route.hpp` — 扩展接口
  `drain(timeout)` + `close()`；原 `stop()` 严格语义。
- `src/bind/bind_module.cpp` — 暴露 `PcmFormat` (POD 值类型) +
  `IAudioRoute` (trampoline base) + `WasapiAudioRoute` +
  `FakeAudioRoute` + `upsample_16k_to_48k` (test-only via `_C`) +
  `UpsampleState` (test-only via `_C`) + `FakeAudioRoute` introspection
  (`recorded_samples_list` / `peak` / `rms`)。
- `apps/windows/rc003/src/ovb_rc003/audio_route_native.py` —
  `make_audio_route(endpoint_name, host_api_name)` 工厂 + `_NativeAudioRoute`
  桥接 shim；`shadow` 禁止 per plan §3 rule 5。
- `apps/windows/rc003/src/remotemic_native/__init__.py` —
  `PcmFormat` + `WasapiAudioRoute` 公开 re-export（ADR-0011
  single-import-surface）。
- `apps/windows/rc003/tests/fakes/audio_route_fakes.py` (NEW) —
  `FakePlaybackSink` Python recording double（**不**入生产代码）；
  与 `FakeAudioRoute` 1:1 对齐。
- `tests/unit/test_bounded_pcm_queue.cpp` — drop-oldest 边界 +
  累积 + 线程安全 + 越界参数。
- `tests/unit/test_pcm_chunker.cpp` — 20 ms 块边界 +
  silence-padded + 残余 flush。
- `tests/unit/test_upsample_16k_to_48k.cpp` — 8 个脚本 byte-exact
  对齐 Python `audio_playback.py:154-172`。
- `tests/unit/test_fake_audio_route.cpp` — lifecycle counter +
  drop-oldest + recorded_samples 序列。
- `tests/unit/test_wasapi_audio_route.cpp` — Windows-only，
  Linux/macOS CI 用 `FakeAudioRoute` 替代。
- `tests/bind/test_audio_route_bind_smoke.py` — pybind11 binding
  smoke（G3）。
- `tests/bind/test_upsample_16k_to_48k_parity.py` (NEW) — 8 个
  byte-exact parity 脚本（G3）。
- `apps/windows/rc003/tests/test_audio_route_native_parity.py` (NEW) —
  10 个场景 + 2 个 sanity，driver identical scripts through
  both recording doubles；6 项对齐（sample-count / peak / RMS /
  drop-count / drain-order / lifecycle counter）。
- `apps/windows/rc003/tests/test_phase4_audio_route_native_switch.py`
  (NEW) — 9 个测试（DefaultDispatch / NativeDispatch /
  RestoreAfterUnset / SingleOwner）；env-leak safety via
  snapshot+restore `_EnvCase`（5ce9bd5 corrective pattern）。
- `apps/windows/rc003/tests/test_phase4_audio_route_production_routing.py`
  (NEW) — 4 个 source-level 测试；defense in depth against a
  future commit re-introducing a direct `EndpointPlaybackSink(...)`
  reference in `app.py`。
- `tools/verify_phase4_audio_parity.py` (NEW) — 2 个 parity gate
  接受证。
- `tools/verify_phase4_native_switch.py` (NEW) — 4-condition
  acceptance proof，镜像 `verify_phase3_production_routing.py`
  pattern。
- `tools/run_parity_test.py` (REWRITTEN) — 之前的 wrapper 重复
  `-m -` argv；argparse 拒绝重复；ctest 把 exit 2 当 Passed
  报告（Phase 3 parity 被掩盖）。新 wrapper in-process 解析，
  strip leading `-m unittest`，programmatic `unittest.main` +
  `TestLoader.discover`（`-s/-t/-p` 触发），直接 `sys.path`
  修改。**同时修复 Phase 3 + Phase 4 parity。**
- `docs/testing/PHASE4-REAL-ACCEPTANCE.md` (NEW) — G6 手动
  验收程序（RC003 + VB-Cable + Typeless / Qianwen），映射
  每个 audio lifecycle event 到 log-line + Typeless / Qianwen
  行为检查 + restore-to-default + recording template。
- `CMakeLists.txt` — `remotemic_audio` 库（5 个 .cpp）加入构建；
  5 个 ctest 单元测试目标；5 个 G3 binding smoke 目标；
  4 个 G3 byte-exact parity 目标；2 个 G5 native switch +
  production routing 目标；`_REMOTEMIC_PARITY_HELPER` /
  `_REMOTEMIC_PARITY_ENV` 定义上移到所有 `add_test()` 之前
  （之前 Phase 4 step 4 add_test 触发时未定义）。

未跑 / 留待（Phase 4 closeout 之后下一步）：

- G6（真机 + Typeless 完整链路 per `docs/testing/PHASE4-REAL-ACCEPTANCE.md`）
  — 当前环境无 RC003 + Typeless + 软件 VB-Cable 同时具备；本次
  会话仅完成 ADR-0014 §10 列出的"自动化 + 文档"两半门禁，真机
  端到端观察 deferred 至下一次有硬件的会话。
- G6 配套：Qianwen Frida adapter work 明确 NOT in Phase 4 scope
  （per ADR-0014 §10 + user direction "先不管千问，这个有点复杂"）；
  结构性问题与 Phase 3 同源（已 deferred，见 `[0.4.0-candidate]`
  CHANGELOG entry）。
- G4（PyInstaller frozen `--dry-run` / Inno Setup installer / 便携
  ZIP / 签名发布）— 在 Phase 4 closeout commit 完成 + 版本号
  bump 到 `0.5.0-candidate` 之后，按 `cpp-migration-version-policy.md`
  Rule 1 单独安排。

ADR-0014 状态：本次提交把状态从 `proposed` → `accepted`，与
Phase 3 step 6 closeout（commit `11f58bd`）相同的"All G1/G2/G3/G5
green; G6 deferred; version bump 0.4.0 → 0.5.0" 节奏。

行为变化：**无**。默认实现仍是 Python（`audio_playback.EndpointPlaybackSink`）；
只有 `REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native` 显式选择才会调用
C++ 路径。**`shadow` 不允许**（plan §3 rule 5：WASAPI 是 side-effecting
设备句柄）。

---

## [0.4.0-candidate] — 2026-08-30 — Phase 3 implementation complete (automated gates passed; real RC003 / Typeless / Qianwen acceptance deferred)

**阶段状态：implementation complete；automated gates passed；real
RC003 / Typeless / Qianwen acceptance deferred。** Phase 3 不视为
完全关闭，直至真机 + Typeless + Qianwen 至少各跑过一次并观察。

**版本号说明**：按 `memory/cpp-migration-version-policy.md` Rule 2
预先 bump `0.3.0-candidate → 0.4.0-candidate`，同步
`CMakeLists.txt` 的 `project(VERSION ...)`、
`apps/windows/rc003/src/ovb_rc003/__init__.py` 的 `__version__`、
`apps/windows/rc003/pyproject.toml` 的 `version`。
`installer/RemoteMicRC003Setup.iss` 的 `AppVersion` 故意不动
（per Rule 1 — packaging 留给 phase 8）。即使真机验收 deferred，
版本号不回退，因为它跟踪的是 implementation+test 状态；后续若
真机验收发现 regression，按既定策略处理：先修复 + 复测，再按
`memory/cpp-migration-version-policy.md` Rule 1/2 决定是否需要
patch bump，而不是预先设定。

**范围**：9 阶段路线图第 3 阶段（C++/CPython 迁移第 5 区：
VoiceController 状态机 + ATVV 会话边界 + 释放消抖 +
AUDIO_STOP 2500 ms 稳定停止回退 + 关闭重试所有权）。

**状态对象 shipping 路径已接入工厂**（产品路径现在通过
`make_voice_controller()` / `make_voice_edge_debouncer()` /
`make_atvv_session()` 三个工厂 dispatch，对应
`app.py` 的 `self._voice` / `self._voice_edge_debouncer` 构造点和
`ble_transport_winrt.RC003BleSession.__init__` 的 `self._session`
构造点；默认全部 `python`，所以普通用户的体验与 Phase 2 完全一致；
设置 `REMOTEMIC_NATIVE_CHOICE_*=native` 时真实产品路径得到
`_Native*` 桥接包装（单 owner，禁止 python/native 双跑）。
Phase 3 真实路由改动的门禁见下表 G7 行）。

详见 [ADR-0013](../../docs/decisions/ADR-0013-phase3-session-state-machine-boundary.md)
第 1-9 节 + Phase 3 step 1-6 commits。

### Phase 3 / Area — VoiceController / ATVV session 边界 + 释放消抖 + AUDIO_STOP 回退（C++ 迁移）

完成 Phase 3（ADR-0013）：把 Python 基线
`ovb_rc003.voice_controller.VoiceController`、
`ovb_rc003.voice_edge_debouncer.VoiceEdgeDebouncer`、
`ovb_rc003.atvv_session.ATVVSession` 三者的状态机行为迁移到
C++：

- `remotemic::voice::VoiceController` — Toggle/Hold 二选 + holding /
  toggle_active 两个状态位；`on_mic_button_pressed` /
  `on_audio_stopped` / `reset` / `restore_pending` /
  `cancel_pending` 一一对应 python 实现；`restore_pending` 仅接受
  KeyUp / Tap 两种关闭动作形态（XRBM-019 close-retry ownership）。
- `remotemic::voice::VoiceEdgeDebouncer` — 200 ms 默认释放窗口（可注入
  `[50ms, 500ms]` 区间），mutex 保护 + `release_seq` 单调计数器使 in-flight
  handler 在新 release 或 `shutdown` 之后失效；`ClockFn` /
  `TimerFactory` 全部可注入（C++ 端默认 noop；bridge wrapper
  提供 `threading.Timer` 桥接）。
- `remotemic::atvv::Session` — `handle_control` 派发 Caps /
  MicButton / AudioStart / AudioStop / AudioSync / Unknown 六种
  事件；late-audio guard 通过注入的 `ClockFn` 暴露（默认
  2500 ms = `LATE_AUDIO_GUARD_SECONDS`）；`handle_audio` 串起
  Phase 2 的 PCM 管线（FrameAccumulator → ImaDecoder →
  DcHighPassFilter → postprocess），caps 变化时按 sample_rate
  重建 DcHighPassFilter。`mic_open_command` /
  `mic_close_command` 携带协商版本号 + 最近 session_id，与 Python
  实现字节级一致。

Python ↔ C++ 字节级一致（无容差）：

- 8 个 VoiceController 状态机脚本（press / stop / reset /
  restore_pending / cancel_pending × Toggle / Hold 组合）
- 5 个 VoiceEdgeDebouncer 脚本（release / press / shutdown /
  fire × 多次 release / release-press-release）
- 4 个 ATVVSession 脚本（caps + start + stop / mic_button /
  short audio_sync / caps+start+stop+audio inside late-audio guard）

门禁（ADR-0013 §6 + cpp-migration-version-policy.md Rule 1/2，
全部通过）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 (C++) | `ctest -C Debug   -R '^(remotemic_voice_controller_tests\|remotemic_edge_debouncer_tests\|remotemic_session_tests)\$'` | 3/3 通过 |
| G1 (C++) | `ctest -C Release -R '^(remotemic_voice_controller_tests\|remotemic_edge_debouncer_tests\|remotemic_session_tests)\$'` | 3/3 通过 |
| G2 (Phase 2 全部 4 区不退化) | `ctest -C Debug   -R '^(remotemic_unit_tests\|remotemic_atvv_tests\|remotemic_atvv_control_tests\|remotemic_adpcm_ima_tests\|remotemic_adpcm_dc_tests\|remotemic_adpcm_postprocess_tests\|remotemic_adpcm_frame_tests)\$'` | 7/7 通过 |
| G2 (Phase 2 全部 4 区不退化) | `ctest -C Release -R '^(remotemic_unit_tests\|remotemic_atvv_tests\|remotemic_atvv_control_tests\|remotemic_adpcm_ima_tests\|remotemic_adpcm_dc_tests\|remotemic_adpcm_postprocess_tests\|remotemic_adpcm_frame_tests)\$'` | 7/7 通过 |
| G3 (binding smoke) | `ctest -C Debug   -R '^(remotemic_voice_controller_bind_smoke\|remotemic_voice_edge_debouncer_bind_smoke\|remotemic_atvv_session_bind_smoke)\$'` | 3/3 通过 |
| G3 (binding smoke) | `ctest -C Release -R '^(remotemic_voice_controller_bind_smoke\|remotemic_voice_edge_debouncer_bind_smoke\|remotemic_atvv_session_bind_smoke)\$'` | 3/3 通过 |
| G5 (shadow parity) | `ctest -C Debug   -R '^(remotemic_voice_controller_native_parity\|remotemic_voice_edge_debouncer_native_parity\|remotemic_atvv_session_native_parity)\$'` | 3/3 通过 |
| G5 (shadow parity) | `ctest -C Release -R '^(remotemic_voice_controller_native_parity\|remotemic_voice_edge_debouncer_native_parity\|remotemic_atvv_session_native_parity)\$'` | 3/3 通过 |
| G6 (native switch + fake backend) | `ctest -C Debug   -R '^remotemic_phase3_native_switch\$'` | 1/1 通过 |
| G6 (native switch + fake backend) | `ctest -C Release -R '^remotemic_phase3_native_switch\$'` | 1/1 通过 |
| G7 (production routing closeout) | `python tools/verify_phase3_production_routing.py` | 4/4 条件全部 PASS（默认 = python / 三键 native → shim / 恢复 → python / 无双 owner） |
| G7 (production routing closeout) | `ctest -C Debug   -R '^remotemic_phase3_production_routing\$'` | 17/17 通过（1 skipped：本地无 `_C.pyd` 运行时降级到 python fallback；Windows runner with `_C.pyd` 验证 C++ 端） |
| G7 (production routing closeout) | `ctest -C Release -R '^remotemic_phase3_production_routing\$'` | 同上 |
| 总计 | `ctest -C Debug` | 25/25 通过 |
| 总计 | `ctest -C Release` | 25/25 通过 |

### 真机 / 第三方验收（**deferred**）

以下三块尚未在本会话环境中真实执行并观察，因此记为 deferred。
**Phase 3 不视为完全关闭**直至这三块至少各跑过一次：

| 验收项 | 状态 | 说明 |
|---|---|---|
| RC003 真机端到端 | **deferred** | 当前环境无 RC003 设备 / 未真实跑一次物理 mic press → AudioStart → AudioStop → host hotkey 路径。状态机迁移是否引入回归需要真机观察。 |
| Typeless 集成验收 | **deferred** | Typeless 工具未在本环境内运行；语音快捷键落入 Typeless 输入路径的端到端 flow 未真实观察。 |
| Qianwen 集成验收 | **deferred** | Qianwen 集成未在本环境内运行；流式 PCM 经过 VoiceController / Session 边界后到达 Qianwen ASR 的端到端 flow 未真实观察。 |

只有在这三块至少各跑过一次、observation 记录在 issues / log
后，才能把 `deferred` 改为 `passed`，并在后续 PR 中正式把 Phase 3
标记为完全关闭。当前版本 `0.4.0-candidate` 不回退，但**也不能**
误读为"Phase 3 已完全 closed"。

---

## [0.3.0-candidate] — 2026-08-30 — Phase 2 complete (4 areas)

里程碑：9 阶段路线图第 2 阶段（C++/CPython 迁移 4 区全部完成且通过
exit review 与 provenance 核验）。版本号按
`memory/cpp-migration-version-policy.md` Rule 2 bump
`0.2.0-candidate → 0.3.0-candidate`，同步 `CMakeLists.txt` 的
`project(VERSION ...)`、`apps/windows/rc003/src/ovb_rc003/__init__.py`
的 `__version__`、`apps/windows/rc003/pyproject.toml` 的 `version`。
`installer/RemoteMicRC003Setup.iss` 的 `AppVersion` 故意不动
（per Rule 1 — packaging 留给 phase 8）。

详见各 area 小节 + Phase 2 closeout corrective（commit `5ce9bd5`）。

### Phase 2 / Area 4 — ADPCM DC 高通 + 平滑增益 + `FrameAccumulator`（C++ 迁移）

完成 Phase 2 第 4 区（[ADR-0012](../../docs/decisions/ADR-0012-atvv-adpcm-phase2-boundary.md)
第 3 节的 `remotemic::adpcm::DcHighPassFilter` / `postprocess` /
`FrameAccumulator`）。Python 基线
`ovb_rc003.atvv_protocol.DCHighPassFilter` / `postprocess` /
`FrameAccumulator` 与 C++ 实现 `remotemic::adpcm::DcHighPassFilter` /
`postprocess` / `FrameAccumulator` 样本级一致（PCM int16 无容差、
`FrameAccumulator.emit` 字节无容差）。状态对象在 Python 与
C++ 之间 byte-equal 重置与 replay 后保持一致。

**Phase 2 全部 4 区已完成；本提交仍不升版本号**（按
`memory/cpp-migration-version-policy.md` Rule 1/2：单一一次 Phase 2
closeout commit 之后才一次性 bump `0.2.0-candidate → 0.3.0-candidate`
+ 更新 `installer/RemoteMicRC003Setup.iss` 的 `AppVersion`；状态对象
shipping 路径不掺入）。

门禁（ADR-0012 §8，全部 6 个 area × Debug + Release + smoke + 4 区
shadow parity 同时满足）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 | `ctest -C Debug   -R '^(remotemic_atvv_tests\|remotemic_atvv_control_tests\|remotemic_adpcm_ima_tests\|remotemic_adpcm_dc_tests\|remotemic_adpcm_postprocess_tests\|remotemic_adpcm_frame_tests)\$'` | 6/6 通过 |
| G2 | 同 `--config Release` | 6/6 通过 |
| G3 | `ctest -C Release -R '^(remotemic_atvv_bind_smoke\|remotemic_atvv_control_bind_smoke\|remotemic_adpcm_ima_bind_smoke\|remotemic_adpcm_dc_bind_smoke\|remotemic_adpcm_postprocess_bind_smoke\|remotemic_adpcm_frame_bind_smoke)\$'` | 6/6 通过（含 DC / FrameAccumulator 的 `reset()` parity、`pending_size < frame_size <= 65535` invariant、`frame_size <= 0` no-op + `> 65535` 显式 `TypeError` 公共 API 边界） |
| G5 | `REMOTEMIC_NATIVE_CHOICE_ADPCM_DC_HIGHPASS=shadow REMOTEMIC_NATIVE_CHOICE_ADPCM_POSTPROCESS=shadow REMOTEMIC_NATIVE_CHOICE_ADPCM_FRAME_ACCUMULATOR=shadow python -m unittest -p test_atvv_native_parity_area4.py` | 8/8 通过 |
| G5 | 顺带回归 G5 1/2/3 区（见下方"未跑 / 留待 / 已回归"小节） | 4 区 16/16 通过 |

`frame_size` 公共 API 合同（[ADR-0012 §3.1](../../docs/decisions/ADR-0012-atvv-adpcm-phase2-boundary.md)，
本次最终态）：

| 输入 | 行为 |
|---|---|
| `<= 0` | binding 层 no-op：`[]` 返回、`pending_size` 不变（与 Python 基线 `if frame_size <= 0: return []` 守卫逐字对应） |
| `1..65535` | narrow 到 `std::uint16_t`，委托给 C++ core |
| `> 65535` | 显式 `TypeError`，数据**不入 C++**，**`pending_size` 也不变** |

C++ core `remotemic::adpcm::FrameAccumulator::append(span<const u8>, std::uint16_t)`
签名**未变**；规范化发生在 `src/bind/bind_module.cpp` 的
`FrameAccumulator.append` binding lambda 上。`reset()` 合同
（`pending_.clear()` 在 O(1) 内、保留 `capacity()`；与全新构造实例在
相同输入/frame_size 下 byte-equal）同时在 C++ 单元测试与 Python
binding smoke 上 parity 验证。

新增内容：

- `include/remotemic/adpcm/dc_highpass.hpp` / `src/adpcm/dc_highpass.cpp`
  — `remotemic::adpcm::DcHighPassFilter`（单极 IIR，alpha =
  `exp(-2π·fc/fs)`；`reset()` 把 `previous_input_` / `previous_output_`
  清零并把 `initialized_` 翻回 `false`）。
- `include/remotemic/adpcm/postprocess.hpp` / `src/adpcm/postprocess.cpp`
  — `remotemic::adpcm::postprocess(span<const i16>, gain_db)`
  自由函数；3-tap smoothing + 增益 clamp `[-24, +24]` dB + `NaN` /
  `±inf` 视作 0 + 输出 clamp 到 int16。
- `include/remotemic/adpcm/frame_accumulator.hpp` /
  `src/adpcm/frame_accumulator.cpp` — `remotemic::adpcm::FrameAccumulator`
  值类型，含 `append` / `reset` / `pending_size` 三个公开成员。
- `tests/unit/test_adpcm_dc_highpass.cpp` — 5 个 gold fixture
  （`dc-*.json`）+ 3 个 `reset()` parity 子测试，循环 1/1 通过。
- `tests/unit/test_adpcm_postprocess.cpp` — 10 个 gold fixture
  （`postprocess-*.json`，含 `NaN` / `+inf` / `-inf` gain_db），循环
  1/1 通过。
- `tests/unit/test_adpcm_frame_accumulator.cpp` — 7 个 gold fixture
  （`frame-*.json`）+ `reset()` carry-over / `frame_size=0` / 边界
  65535 / invariant `pending_size < frame_size <= 65535` 共 4 类
  子测试，循环 1/1 通过。
- `tests/bind/test_adpcm_dc_bind_smoke.py` /
  `tests/bind/test_adpcm_postprocess_bind_smoke.py` /
  `tests/bind/test_adpcm_frame_bind_smoke.py` —
  pybind11 绑定烟雾（去糖后含 `reset()` parity、`pending_size` 不变
  边界、`<=0/1/>65535` 实际抛/返回）。
- `tests/test_atvv_native_parity_area4.py` — 运行时 shadow parity
  测试（G5）：8 个测试覆盖 5 dc + 10 postprocess + 7 frame
  fixture，外加 `reset_state_parity`（after warmup + no-warmup）、
  `partial_across_calls`、`no_op_contract_parity`（`<=0` no-op +
  pending 不变）；strict value-exact（mismatch 即停并报告 fixture +
  双方输出 + 首差异 index）。
- `apps/windows/rc003/src/ovb_rc003/atvv_native_bridge.py` —
  `apply_dc_highpass` / `postprocess_pcm` / `accumulate_frames`
  三态切换包装（独立 switch 名 `adpcm_dc_highpass` /
  `adpcm_postprocess` / `adpcm_frame_accumulator`）。
- `apps/windows/rc003/src/ovb_rc003/_remotemic_native_runtime.py` —
  三个新 key 注册到默认策略表（默认 `python`）。
- `apps/windows/rc003/src/bind/bind_module.cpp` —
  `FrameAccumulator.append` binding 接受 Python `int`，规范化 `<=0`
  / `>65535` 后再 narrow 到 `std::uint16_t` 委托给 C++ core；C++
  core 签名不变。`FrameAccumulator.reset()` 在 binding 暴露，
  文档明确不抛。
- `apps/windows/rc003/src/remotemic_native/__init__.py` — 新增
  `apply_dc_highpass` / `postprocess` / `FrameAccumulator` /
  `DcHighPassFilter` 的 re-export。
- `apps/windows/rc003/tests/fixtures/atvv/` 新增 22 个 synthetic
  JSON 夹具（`dc-*.json` ×5、`postprocess-*.json` ×10、
  `frame-*.json` ×7）。所有夹具 100% synthetic，无任何捕获的
  真实设备或语音数据。

未跑 / 留待（Phase 2 closeout 之后下一步）：

- G4（`tests/test_atvv_protocol.py` + `tests/test_atvv_golden_fixture.py`
  默认 python 模式 100% 通过）— 期间 Python 基线**未改动**（按
  ADR-0012 §3 Non-goals：保持基线 API 形状，迁入只增 C++ 路径）。
- G6（PyInstaller frozen `--dry-run` / Inno Setup installer / 便携 ZIP
  / 签名发布）— 在 Phase 2 全部 4 区 closeout commit 完成 + 版本号
  bump 到 `0.3.0-candidate` 之后，按 `cpp-migration-version-policy.md`
  Rule 1 单独安排。

ADR-0012 状态：本次提交把状态从 `proposed` → `accepted`，并于
step 4 corrective（commit `61a3636`）补强 §3.1（`frame_size` 公共 API
合同表）与 §3.2（area 4 范围重申），step 5（commit `73e9155`）补强
G5（4 区分组 parity 命令）。

行为变化：**无**。默认实现仍是 Python；只有 `native` 或 `shadow`
显式选择才会调用 C++ 路径。

### Phase 2 / Area 3 — IMA/DVI ADPCM 解码器（C++ 迁移）
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

### Phase 2 / Area 2 — ATVV control message 编解码（C++ 迁移）

完成 Phase 2 第 2 区（[ADR-0012](decisions/ADR-0012-atvv-adpcm-phase2-boundary.md)
第 3 节的 `remotemic::atvv::ControlMessage` 与 `mic_open_command` /
`mic_close_command`）。Python 基线
`ovb_rc003.atvv_protocol.parse_control_payload` /
`mic_open_command` / `mic_close_command` 与 C++ 实现
`remotemic::atvv::parse_control_message` /
`remotemic::atvv::mic_open_command` /
`remotemic::atvv::mic_close_command` 字节级一致。**Phase 2 全部 4 区
尚未完成，版本号不升**；下次小版本号 `0.3.0-candidate` 留给 Phase 2
closeout。

门禁（ADR-0012 §8）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 | `ctest -C Debug   -R '^remotemic_atvv_control_tests\$'`          | 1/1 通过 |
| G2 | `ctest -C Release -R '^remotemic_atvv_control_tests\$'`          | 1/1 通过 |
| G3 | `ctest -C Debug   -R '^remotemic_atvv_control_bind_smoke\$'`     | 1/1 通过（12/12 子测试） |
| G3 | `ctest -C Release -R '^remotemic_atvv_control_bind_smoke\$'`     | 1/1 通过（12/12 子测试） |
| G5 | `REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_PARSE=shadow REMOTEMIC_NATIVE_CHOICE_ATVV_CONTROL_ENCODE=shadow python -m unittest -p test_atvv_native_parity_control.py` | 4/4 通过，12 个夹具全部 byte-exact |
| G5 | `python -m unittest -p test_atvv_native_parity.py` （Area 1 回归） | 2/2 通过 |

新增内容：

- `include/remotemic/atvv/control.hpp` — `Opcode` 枚举、
  `ControlMessage = std::variant<CapsPayload, MicButtonPayload,
  AudioStartPayload, AudioStopPayload, AudioSyncPayload, UnknownPayload>`、
  `parse_control_message(std::span<const std::uint8_t>) noexcept ->
  std::optional<ControlMessage>`、`mic_open_command` /
  `mic_close_command` host→device 编码器。
- `src/atvv/control.cpp` — 与 Python 基线逐字节匹配的解析与编码。
  未知 opcode 包成 `UnknownPayload`（保留原始字节），长度 < 7 的
  `AUDIO_SYNC` 也走 `UnknownPayload`（保持 state machine 看到一致
  的 `ControlMessage`，由它决定是否丢弃）。
- `tests/unit/test_atvv_control.cpp` — CTest 单元测试，从同一套
  JSON 夹具读 12 个子测试（4 encode + 8 decode）。
- `tests/bind/test_atvv_control_bind_smoke.py` — pybind11 绑定
  烟雾测试。`std::variant` 在 pybind11 边界上转为 dict（`opcode`
  + per-opcode 字段），键集与 C++ 单元测试一致以便对照。
- `tests/test_atvv_native_parity_control.py` — 运行时 shadow parity
  测试（Python 与 C++ 全部 byte-exact，0 容差）。
- `apps/windows/rc003/src/ovb_rc003/atvv_protocol.py`：
  新增 `parse_control_payload(data) -> Optional[dict]`，与 C++
  返回的 dict 形状逐键对齐，shadow 比对无需 translation 层。
- `apps/windows/rc003/src/ovb_rc003/atvv_native_bridge.py`：
  新增 `parse_control` / `mic_open_command` / `mic_close_command`
  三态切换包装（独立 switch 名 `atvv_control_parse` /
  `atvv_control_encode`，允许 Phase 2 closeout 之后单独打开）。
- `apps/windows/rc003/src/ovb_rc003/_remotemic_native_runtime.py`：
  `atvv_control_parse` / `atvv_control_encode` 注册到默认策略表
  （默认 `python`），env 变量名拼写错误时立刻失败而非静默回落。
- `apps/windows/rc003/tests/fixtures/atvv/` 新增 12 个 synthetic
  JSON 夹具（4 encode + 8 decode），并修正 `control-decode-audio-
  start-with-sid.json` 与 `control-decode-audio-sync.json` 的 input
  长度（之前描述与实际字节数对不上，已按描述补足字节）。所有夹具
  100% synthetic，无任何捕获的真实设备或语音数据。

行为变化：**无**。默认实现仍是 Python；只有 `native` 或 `shadow`
显式选择才会调用 C++ 路径。

### Phase 2 / Area 3 — IMA/DVI ADPCM 解码器（C++ 迁移）

完成 Phase 2 第 3 区（[ADR-0012](decisions/ADR-0012-atvv-adpcm-phase2-boundary.md)
第 3 节的 `remotemic::adpcm::ImaDecoder`）。Python 基线
`ovb_rc003.atvv_protocol.IMAADPCMDecoder` 与 C++ 实现
`remotemic::adpcm::ImaDecoder` **逐样本一致**（无容差，PCM
sample-exact）。**Phase 2 全部 4 区尚未完成，版本号不升**；下次小
版本号 `0.3.0-candidate` 留给 Phase 2 closeout。

门禁（ADR-0012 §8）：

| 门禁 | 命令 | 结果 |
|---|---|---|
| G1 | `ctest -C Debug   -R '^remotemic_adpcm_ima_tests\$'`        | 1/1 通过（11/11 子测试） |
| G2 | `ctest -C Release -R '^remotemic_adpcm_ima_tests\$'`        | 1/1 通过（11/11 子测试） |
| G3 | `ctest -C Debug   -R '^remotemic_adpcm_ima_bind_smoke\$'`   | 1/1 通过 |
| G3 | `ctest -C Release -R '^remotemic_adpcm_ima_bind_smoke\$'`   | 1/1 通过 |
| G5 | `REMOTEMIC_NATIVE_CHOICE_ADPCM_IMA_DECODE=shadow python -m unittest -p test_atvv_native_parity_adpcm.py` | 2/2 通过，11 个夹具全部 sample-exact |
| G5 | `python -m unittest -p test_atvv_native_parity.py` （Area 1 回归） | 2/2 通过 |
| G5 | `python -m unittest -p test_atvv_native_parity_control.py` （Area 2 回归） | 4/4 通过 |

关键修复：**int16 溢出**。`decode_nibble` 的 `difference` 与
`predictor_` 累加器在 `step_index` 较大时（典型值 step_table[80]
= 15289，`step + step/2 + step/4 + step/8` ≈ 28666）会超过 int16
上限 32767。Python 用无界 int 永远不溢出；C++ 必须先在 int32 空间
累加并 clamp，再 cast 到 int16。否则 cast 时静默回绕（mod 65536），
导致预测器半途翻号，sample-exact 立即失败。**同时**，必须先 clamp
int32 值再 cast —— 直接 `static_cast<int16_t>(next)` 即使后面再
调 `clamp_predictor` 也没用，因为 cast 后的回绕值已经在 int16
范围内，clamp 不会触发。

新增内容：

- `include/remotemic/adpcm/ima_decoder.hpp` — `remotemic::adpcm::
  ImaDecoder` 值类型：`reset(predictor, step_index)`（clamp 到
  `[-32768, 32767]` / `[0, 88]`），`decode(span<const u8>) ->
  std::vector<int16_t>`（每字节 2 样本，高 nibble 在前）。
- `src/adpcm/ima_decoder.cpp` — 与 Python 基线逐样本匹配的解码器
  （非标准 ATVV bit mapping：bit 0 = step/4，不是更常见的
  step/8，因为上游 Xiaomi Mi Box 设备实际就是这个 flavor）。
- `tests/unit/test_adpcm_ima.cpp` — CTest 单元测试，从同一套
  JSON 夹具读 11 个子测试。
- `tests/bind/test_adpcm_ima_bind_smoke.py` — pybind11 绑定烟雾
  测试。
- `tests/test_atvv_native_parity_adpcm.py` — 运行时 shadow parity
  测试。
- `apps/windows/rc003/src/ovb_rc003/atvv_native_bridge.py`：新增
  `decode_adpcm_frame(data, predictor=0, step_index=0) -> list[int]`
  三态切换包装（独立 switch 名 `adpcm_ima_decode`）。
- `apps/windows/rc003/src/ovb_rc003/_remotemic_native_runtime.py`：
  `adpcm_ima_decode` 注册到默认策略表（默认 `python`）。
- `apps/windows/rc003/src/remotemic_native/__init__.py`：
  re-export `ImaDecoder`。
- `apps/windows/rc003/tests/fixtures/atvv/` 新增 11 个 synthetic
  JSON 夹具（`adpcm-*.json`）+ 1 个 build-time-only 生成器脚本
  `_gen_adpcm_fixtures.py`（不在 unittest 发现范围内）。所有夹具
  100% synthetic，无任何捕获的真实设备或语音数据。

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
