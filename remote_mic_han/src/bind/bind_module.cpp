#include <pybind11/pybind11.h>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "remotemic/bind/errors.hpp"
#include "remotemic/bind/probe_types.hpp"
#include "remotemic/atvv/capabilities.hpp"
#include "remotemic/atvv/control.hpp"
#include "remotemic/atvv/session.hpp"
#include "remotemic/adpcm/ima_decoder.hpp"
#include "remotemic/adpcm/dc_highpass.hpp"
#include "remotemic/adpcm/postprocess.hpp"
#include "remotemic/adpcm/frame_accumulator.hpp"
#include "remotemic/voice/voice_controller.hpp"
#include "remotemic/voice/edge_debouncer.hpp"
#include "remotemic/interfaces/audio_route.hpp"
#include "remotemic/audio/wasapi_audio_route.hpp"
#include "remotemic/audio/fake_audio_route.hpp"
#include "remotemic/audio/upsample_16k_to_48k.hpp"
#include "remotemic/input/input_event.hpp"
#include "remotemic/input/i_input_source.hpp"
#include "remotemic/input/i_host_action_sink.hpp"
#include "remotemic/input/action_resolver.hpp"
#include "remotemic/input/hotkey_physicalizer.hpp"
#include "remotemic/input/fake_input_source.hpp"
#include "remotemic/input/fake_host_action_sink.hpp"

#ifdef _WIN32
#include "remotemic/input/raw_input_source.hpp"
#include "remotemic/input/low_level_keyboard_hook.hpp"
#include "remotemic/input/frida_hid_tap_source.hpp"
#include "remotemic/input/send_input_action_sink.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <variant>

namespace py = pybind11;
using namespace remotemic;
namespace audio = remotemic::audio;
namespace input = remotemic::input;

PYBIND11_MODULE(_C, m) {
    m.doc() = "RemoteMic Windows native binding - private implementation "
              "detail. Imported only via the public remotemic_native "
              "package wrapper. See "
              "docs/decisions/ADR-0011-cpp-python-binding-and-error-model.md";

    // Compile-time version, kept in sync with CMakeLists.txt PROJECT_VERSION
    // and the Python __version__ in apps/windows/rc003/src/ovb_rc003/__init__.py
    // per project policy cpp-migration-version-policy.
    m.attr("__version__") = REMOTEMIC_VERSION;

    // ------------------------------------------------------------------
    // Error translation. Per ADR-0011 §2:
    //   - std::invalid_argument becomes ValueError carrying what()
    //   - remotemic::Error becomes a RuntimeError carrying what()
    //   - any other std::exception also becomes RuntimeError with what()
    //   - nothing is silently swallowed
    // The dedicated `RemoteMicError` Python class with `code` attribute is
    // deferred to the next iteration; the smoke gate only requires that
    // `what()` survives the boundary and that the translator never crashes.
    // ------------------------------------------------------------------
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) {
                std::rethrow_exception(p);
            }
        } catch (const std::invalid_argument& e) {
            // Match the python-baseline's ValueError contract for
            // argument-validation failures (e.g. release_window out
            // of [50ms, 500ms] in VoiceEdgeDebouncer).
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const Error&) {
            // Distinguish from generic std::exception by re-throwing inside.
            try {
                throw;
            } catch (const Error& e) {
                PyErr_SetString(PyExc_RuntimeError, e.what());
            }
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // ------------------------------------------------------------------
    // 1. Value-type round trip
    // ------------------------------------------------------------------
    py::class_<VersionInfo>(m, "VersionInfo")
        .def_readonly("product", &VersionInfo::product)
        .def_readonly("version", &VersionInfo::version)
        .def_readonly("build_number", &VersionInfo::build_number);

    m.def(
        "probe_value_type",
        []() {
            return VersionInfo{
                .product = "RemoteMicWindows",
                .version = REMOTEMIC_VERSION,
                .build_number = 1,
            };
        },
        "Return a small value-type record; validates struct binding."
    );

    // ------------------------------------------------------------------
    // 2. shared_ptr round trip
    // ------------------------------------------------------------------
    py::class_<Counter, std::shared_ptr<Counter>>(m, "Counter")
        .def(py::init<>())
        .def("increment", &Counter::increment, py::arg("delta") = 1)
        .def("value", &Counter::value);

    m.def(
        "probe_shared_ptr",
        []() { return std::make_shared<Counter>(); },
        "Return a heap-allocated counter; validates shared_ptr holder type."
    );

    // ------------------------------------------------------------------
    // 3. py::function callback
    // ------------------------------------------------------------------
    m.def(
        "probe_callback",
        [](CounterSink sink, std::int64_t payload) {
            if (sink) {
                sink(payload);
            }
            return payload;
        },
        py::arg("sink"),
        py::arg("payload") = 42,
        "Invoke a Python callable from C++; validates py::function lifetime."
    );

    // ------------------------------------------------------------------
    // 4. Thrown remotemic::Error (all four codes exercised below)
    // ------------------------------------------------------------------
    m.def(
        "probe_throw",
        [](ErrorCode code) {
            const char* what = "";
            switch (code) {
            case ErrorCode::InvalidArgument: what = "invalid argument"; break;
            case ErrorCode::NotFound:        what = "not found";         break;
            case ErrorCode::Timeout:         what = "timed out";         break;
            case ErrorCode::Internal:        what = "internal failure";  break;
            case ErrorCode::None:            what = "no error";          break;
            }
            throw Error{code, what};
        },
        py::arg("code"),
        "Throw remotemic::Error with the given code; validates exception translator."
    );

    // ------------------------------------------------------------------
    // ErrorCode enum (so Python tests can iterate codes without magic ints)
    // ------------------------------------------------------------------
    py::enum_<ErrorCode>(m, "ErrorCode")
        .value("None", ErrorCode::None)
        .value("InvalidArgument", ErrorCode::InvalidArgument)
        .value("NotFound", ErrorCode::NotFound)
        .value("Timeout", ErrorCode::Timeout)
        .value("Internal", ErrorCode::Internal);

    // ------------------------------------------------------------------
    // 5. ATVV capability parse (Phase 2 / Area 1, ADR-0012)
    //    Pure-compute, no I/O, no thread, no global state. Returns
    //    std::nullopt on malformed input -> None on the Python side.
    //    Field set mirrors the Python ATVVCapabilities dataclass
    //    (apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:64-71)
    //    byte-for-byte so the runtime shadow parity test can compare
    //    the two implementations without tolerance.
    // ------------------------------------------------------------------
    py::class_<atvv::Capabilities>(m, "AtvvCapabilities")
        .def_readonly("version",        &atvv::Capabilities::version)
        .def_readonly("codecs",         &atvv::Capabilities::codecs)
        .def_readonly("interaction",    &atvv::Capabilities::interaction)
        .def_readonly("frame_size",     &atvv::Capabilities::frame_size)
        .def_readonly("selected_codec", &atvv::Capabilities::selected_codec)
        .def_readonly("sample_rate",    &atvv::Capabilities::sample_rate);

    m.def(
        "atvv_capabilities_parse",
        [](py::bytes data) -> std::optional<atvv::Capabilities> {
            const std::string_view view = data;
            std::span<const std::uint8_t> bytes(
                reinterpret_cast<const std::uint8_t*>(view.data()),
                view.size());
            return atvv::parse(bytes);
        },
        py::arg("data"),
        "Parse an ATVV capability notification payload (opcode 0x0B).\n"
        "Returns None on malformed input (too short, wrong opcode, or\n"
        "legacy version with insufficient length). Otherwise returns an\n"
        "AtvvCapabilities value type matching the Python baseline\n"
        "ATVVCapabilities dataclass byte-for-byte."
    );

    // ------------------------------------------------------------------
    // 6. ATVV control encode + decode (Phase 2 / Area 2, ADR-0012)
    //    parse_control_message returns std::optional<std::variant>; the
    //    std::variant doesn't translate to a single py::class_, so the
    //    binding converts each alternative to a small Python dict
    //    matching the JSON fixtures. The set of dict keys per opcode
    //    matches tests/unit/test_atvv_control.cpp exactly so the bind
    //    smoke can compare them without tolerance. encode functions
    //    return py::bytes so the result is byte-for-byte comparable
    //    with the Python baseline (atvv_protocol.py:48-61).
    // ------------------------------------------------------------------
    m.def(
        "atvv_control_parse",
        [](py::bytes data) -> std::optional<py::dict> {
            const std::string_view view = data;
            std::span<const std::uint8_t> bytes(
                reinterpret_cast<const std::uint8_t*>(view.data()),
                view.size());
            auto msg = atvv::parse_control_message(bytes);
            if (!msg.has_value()) {
                return std::nullopt;
            }
            py::dict out;
            if (std::holds_alternative<atvv::CapsPayload>(*msg)) {
                out["opcode"] = "Caps";
            } else if (std::holds_alternative<atvv::MicButtonPayload>(
                           *msg)) {
                out["opcode"] = "MicButton";
            } else if (std::holds_alternative<atvv::AudioStopPayload>(
                           *msg)) {
                out["opcode"] = "AudioStop";
            } else if (std::holds_alternative<atvv::AudioStartPayload>(
                           *msg)) {
                const auto& p = std::get<atvv::AudioStartPayload>(*msg);
                out["opcode"] = "AudioStart";
                if (p.session_id.has_value()) {
                    out["session_id"] = py::int_(*p.session_id);
                } else {
                    out["session_id"] = py::none();
                }
            } else if (std::holds_alternative<atvv::AudioSyncPayload>(
                           *msg)) {
                const auto& p = std::get<atvv::AudioSyncPayload>(*msg);
                out["opcode"] = "AudioSync";
                out["predictor"] = p.predictor;
                out["step_index"] = p.step_index;
            } else {
                const auto& p = std::get<atvv::UnknownPayload>(*msg);
                out["opcode"] = "Unknown";
                out["raw_opcode"] = p.raw_opcode;
            }
            return out;
        },
        py::arg("data"),
        "Parse a device->host ATVV control payload.\n"
        "Returns None for empty input (state machine raises on None).\n"
        "Otherwise returns a dict with an 'opcode' key plus per-opcode\n"
        "fields. Keys/values match the C++ unit test and the JSON\n"
        "golden fixtures byte-for-byte."
    );

    m.def(
        "atvv_mic_open_command",
        [](std::uint16_t version) -> py::bytes {
            const auto v = atvv::mic_open_command(version);
            return py::bytes(
                reinterpret_cast<const char*>(v.data()), v.size());
        },
        py::arg("version"),
        "Encode a host->device MIC_OPEN command. v1 (>= 0x0100)\n"
        "produces b'\\x0c\\x00'; legacy produces b'\\x0c\\x00\\x00'."
    );

    m.def(
        "atvv_mic_close_command",
        [](std::uint16_t version, std::uint8_t session_id) -> py::bytes {
            const auto v = atvv::mic_close_command(version, session_id);
            return py::bytes(
                reinterpret_cast<const char*>(v.data()), v.size());
        },
        py::arg("version"),
        py::arg("session_id"),
        "Encode a host->device MIC_CLOSE command. v1 (>= 0x0100)\n"
        "produces b'\\x0d' followed by session_id; legacy produces\n"
        "just b'\\x0d' (session_id ignored)."
    );

    // ------------------------------------------------------------------
    // 7. IMA/DVI ADPCM decoder (Phase 2 / Area 3, ADR-0012)
    //    Stateful value type matching
    //    apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:158-207.
    //    Each Python instance owns its own decoder; the binding does
    //    not share state across instances. State is exposed via
    //    reset(predictor, step_index) + decode(data) + read-only
    //    predictor/step_index accessors.
    // ------------------------------------------------------------------
    py::class_<adpcm::ImaDecoder>(m, "ImaDecoder")
        .def(py::init<>(),
             "Construct a decoder with predictor=0, step_index=0.")
        .def("reset",
             &adpcm::ImaDecoder::reset,
             py::arg("predictor") = 0,
             py::arg("step_index") = 0,
             "Reset predictor and step_index. predictor is clamped\n"
             "to [-32768, 32767] and step_index to [0, 88].")
        .def("decode",
             [](adpcm::ImaDecoder& self, py::bytes data) {
                 const std::string_view view = data;
                 std::span<const std::uint8_t> bytes(
                     reinterpret_cast<const std::uint8_t*>(view.data()),
                     view.size());
                 return self.decode(bytes);
             },
             py::arg("data"),
             "Decode a byte stream; returns 2 * len(data) samples.\n"
             "High nibble is decoded first per byte.")
        .def_property_readonly("predictor",
                               &adpcm::ImaDecoder::predictor)
        .def_property_readonly("step_index",
                               &adpcm::ImaDecoder::step_index);

    // ------------------------------------------------------------------
    // 8. DC high-pass + smoothing/gain + FrameAccumulator
    //    (Phase 2 / Area 4, ADR-0012)
    //    All three match
    //    apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:210-285.
    //    DCHighPassFilter and FrameAccumulator are stateful value
    //    types; postprocess is a pure free function.
    // ------------------------------------------------------------------
    py::class_<adpcm::DcHighPassFilter>(m, "DcHighPassFilter")
        .def(py::init<double, double>(),
             py::arg("sample_rate"),
             py::arg("cutoff_hz"),
             "Construct a single-pole high-pass filter with the\n"
             "given sample rate and cutoff frequency. alpha =\n"
             "exp(-2*pi*cutoff_hz/sample_rate).")
        .def("reset", &adpcm::DcHighPassFilter::reset,
             "Reset internal state; the next process() call will\n"
             "re-initialize previous_input from samples[0].")
        .def("process",
             [](adpcm::DcHighPassFilter& self,
                std::vector<std::int16_t> samples) {
                 return self.process(samples);
             },
             py::arg("samples"),
             "Filter the input samples; returns a new vector.\n"
             "Empty input -> empty output.")
        .def_property_readonly("alpha", &adpcm::DcHighPassFilter::alpha)
        .def_property_readonly("sample_rate",
                               &adpcm::DcHighPassFilter::sample_rate)
        .def_property_readonly("cutoff_hz",
                               &adpcm::DcHighPassFilter::cutoff_hz);

    m.def(
        "postprocess",
        [](std::vector<std::int16_t> samples, double gain_db) {
            return adpcm::postprocess(samples, gain_db);
        },
        py::arg("samples"),
        py::arg("gain_db") = 10.0,
        "Apply 3-tap smoothing + dB gain to a sample sequence.\n"
        "Empty input -> empty output. NaN/inf gain_db treated as 0;\n"
        "final gain_db clamped to [-24, +24]; output clamped to int16."
    );

    py::class_<adpcm::FrameAccumulator>(m, "FrameAccumulator")
        .def(py::init<>(),
             "Construct an empty accumulator.")
        .def("append",
             [](adpcm::FrameAccumulator& self,
                py::bytes data,
                py::int_ frame_size_py) {
                 const std::string_view view = data;
                 std::span<const std::uint8_t> bytes(
                     reinterpret_cast<const std::uint8_t*>(view.data()),
                     view.size());
                 // Per ADR-0012 section 3.1, the public API contract
                 // at the Python/native seam is:
                 //   frame_size <= 0       -> no-op ([] returned,
                 //                              pending untouched)
                 //   1..65535              -> protocol valid; narrow
                 //                              to std::uint16_t and
                 //                              delegate to C++
                 //   > 65535               -> explicit TypeError
                 //                              rejection (the
                 //                              protocol domain cap;
                 //                              not silently wrapped)
                 // The C++ core FrameAccumulator::append keeps its
                 // std::uint16_t signature unchanged; the binding
                 // layer is the one place where Python ints of any
                 // sign are normalized into the protocol contract.
                 const long long fs = frame_size_py.cast<long long>();
                 if (fs <= 0) {
                     // Hard no-op. We MUST NOT touch self in this
                     // branch: the contract says "<=0 -> pending
                     // unchanged". Returning a freshly-constructed
                     // empty list (not a std::nullopt) so the
                     // Python side still sees a list, matching the
                     // Python baseline FrameAccumulator.append
                     // (atvv_protocol.py:271-279).
                     return std::vector<std::vector<std::uint8_t>>{};
                 }
                 if (fs > 65535) {
                     PyErr_SetString(
                         PyExc_TypeError,
                         "frame_size must be in 1..65535 (uint16_t "
                         "protocol domain); the value supplied is "
                         "outside the protocol-valid range.");
                     throw py::error_already_set();
                 }
                 return self.append(
                     bytes, static_cast<std::uint16_t>(fs));
             },
             py::arg("data"),
             py::arg("frame_size"),
             "Append data and return any complete frames of\n"
             "frame_size bytes; leftover bytes stay pending.\n"
             "Public-API contract (ADR-0012 section 3.1):\n"
             "  frame_size <= 0     no-op; returns [] and does NOT\n"
             "                      modify pending_size. Matches the\n"
             "                      Python baseline guard at\n"
             "                      atvv_protocol.py:272-273.\n"
             "  1..65535            protocol valid; narrow to uint16_t.\n"
             "  frame_size > 65535  explicitly rejected with TypeError\n"
             "                      (the protocol-domain cap; not\n"
             "                      silently wrapped to a uint16).")
        .def("reset", &adpcm::FrameAccumulator::reset,
             "Discard any pending bytes accumulated from previous\n"
             "calls. After reset() the next append() behaves as if\n"
             "the instance were freshly constructed: no partial\n"
             "frame from a previous stream is carried over. Does\n"
             "not throw.")
        .def_property_readonly("pending_size",
                               &adpcm::FrameAccumulator::pending_size);

    // ------------------------------------------------------------------
    // 9. VoiceController + VoiceEdgeDebouncer (Phase 3 / ADR-0013 §3.1,
    //    §3.2). Both are state machines mirroring
    //    apps/windows/rc003/src/ovb_rc003/voice_controller.py and
    //    voice_edge_debouncer.py. Time-injection + TimerFactory are
    //    intentionally NOT exposed at the binding seam: the bridge
    //    wrapper provides a std::thread-backed TimerFactory internally
    //    so production behavior matches the Python implementation, and
    //    tests can drive the C++ side through the python (pure) impl.
    // ------------------------------------------------------------------
    py::enum_<voice::VoiceTriggerMode>(m, "VoiceTriggerMode")
        .value("Toggle", voice::VoiceTriggerMode::Toggle)
        .value("Hold",   voice::VoiceTriggerMode::Hold);

    py::enum_<voice::VoiceHostAction>(m, "VoiceHostAction")
        .value("Tap",     voice::VoiceHostAction::Tap)
        .value("KeyDown", voice::VoiceHostAction::KeyDown)
        .value("KeyUp",   voice::VoiceHostAction::KeyUp);

    py::class_<voice::VoiceController>(m, "VoiceController")
        .def(py::init<voice::VoiceTriggerMode>(),
             py::arg("mode") = voice::VoiceTriggerMode::Toggle)
        .def("on_mic_button_pressed",
             &voice::VoiceController::on_mic_button_pressed,
             "React to the device's own MIC_BUTTON control opcode.\n"
             "Returns the host action that the caller must dispatch\n"
             "(Tap for TOGGLE press, KeyDown for HOLD press).")
        .def("on_audio_stopped",
             &voice::VoiceController::on_audio_stopped,
             "React to the device's own AUDIO_STOP control opcode.\n"
             "Returns the closing action (Tap for TOGGLE, KeyUp for\n"
             "HOLD) or None if no session is currently open.")
        .def("reset", &voice::VoiceController::reset,
             "Force any outstanding session closed (e.g. on\n"
             "disconnect/shutdown). Returns the closing action that\n"
             "the caller must dispatch, or None if nothing was owed.\n"
             "``active`` is False immediately after this returns.")
        .def("restore_pending",
             &voice::VoiceController::restore_pending,
             py::arg("action"),
             "Undo ``reset`` / ``on_audio_stopped``'s eager state-\n"
             "clearing when the closing action failed to deliver\n"
             "(XRBM-019). Only KeyUp and Tap are accepted; passing\n"
             "KeyDown is a caller bug and is silently ignored.")
        .def("cancel_pending",
             &voice::VoiceController::cancel_pending,
             "Clear an outstanding session WITHOUT emitting a\n"
             "compensating host action (e.g. when the opening\n"
             "delivery itself failed).")
        .def_property_readonly("holding", &voice::VoiceController::holding)
        .def_property_readonly("active",  &voice::VoiceController::active);

    py::class_<voice::VoiceEdgeDebouncer>(m, "VoiceEdgeDebouncer")
        .def(py::init([](std::int64_t release_window_ms) {
                 return std::make_unique<voice::VoiceEdgeDebouncer>(
                     std::chrono::milliseconds(release_window_ms));
             }),
             py::arg("release_window_ms") = 200,
             "Construct a release-window debouncer. The C++ side\n"
             "plugs a no-op TimerFactory at the binding seam; the\n"
             "Python bridge wrapper supplies a std::thread-backed\n"
             "factory so production timing matches the python\n"
             "baseline. release_window must be in [50ms, 500ms].")
        .def("on_press", &voice::VoiceEdgeDebouncer::on_press,
             "Cancel any pending release. No-op if nothing pending.")
        .def("on_release",
             [](voice::VoiceEdgeDebouncer& self,
                std::function<void()> handler) {
                self.on_release(std::move(handler));
             },
             py::arg("handler"),
             "Schedule ``handler`` after release_window. With the\n"
             "binding's no-op timer factory the handler is held in\n"
             "memory; a press / shutdown arriving first cancels it.")
        .def("shutdown", &voice::VoiceEdgeDebouncer::shutdown,
             "Cancel any pending release so a worker thread can\n"
             "exit cleanly.")
        .def("fire_pending_now_for_test",
             &voice::VoiceEdgeDebouncer::fire_pending_now_for_test,
             "Test-only: synchronously fire the pending handler.\n"
             "Returns True if a handler was fired.")
        .def_property_readonly(
            "release_window_ms",
            [](const voice::VoiceEdgeDebouncer& self) {
                return static_cast<std::int64_t>(
                    self.release_window().count());
            });

    // ------------------------------------------------------------------
    // 10. ATVV Session (Phase 3 / ADR-0013 §3.3). Mirrors
    //     apps/windows/rc003/src/ovb_rc003/atvv_session.py:149-249,
    //     wiring the existing Phase 2 decoders
    //     (FrameAccumulator -> ImaDecoder -> DcHighPassFilter ->
    //     postprocess) lazily when caps arrive. ``handle_control``
    //     returns a tagged dict matching the existing
    //     ``atvv_control_parse`` contract (Area 2) so the Python
    //     bridge can compare Python-vs-native without tolerance.
    // ------------------------------------------------------------------
    py::class_<atvv::CapsReceived>(m, "AtvvSessionCapsReceived")
        .def_readonly("capabilities", &atvv::CapsReceived::capabilities);

    py::class_<atvv::MicButtonPressed>(m, "AtvvSessionMicButtonPressed");

    py::class_<atvv::AudioStarted>(m, "AtvvSessionAudioStarted")
        .def_readonly("session_id", &atvv::AudioStarted::session_id);

    py::class_<atvv::AudioStopped>(m, "AtvvSessionAudioStopped");

    py::class_<atvv::AudioSynced>(m, "AtvvSessionAudioSynced");

    py::class_<atvv::UnknownControl>(m, "AtvvSessionUnknownControl")
        .def_readonly("opcode", &atvv::UnknownControl::opcode);

    py::class_<atvv::Session>(m, "AtvvSession")
        .def(py::init<double>(),
             py::arg("gain_db") = 10.0,
             "Construct a session with the given dB gain and\n"
             "the production default late-audio guard (2500 ms).")
        .def(py::init<double, std::chrono::milliseconds,
                      std::function<std::chrono::milliseconds()>>(),
             py::arg("gain_db"),
             py::arg("late_audio_guard_ms"),
             py::arg("clock"),
             "Test-only constructor exposing the late-audio guard\n"
             "and an injected monotonic clock so unit tests can\n"
             "advance time without sleeping.")
        .def_property_readonly(
            "capabilities",
            [](const atvv::Session& s) -> const atvv::Capabilities* {
                return s.capabilities();
            },
            "Negotiated ATVV capabilities, or None if no CAPS has\n"
            "arrived yet.")
        .def_property_readonly("mic_open", &atvv::Session::mic_open)
        .def("handle_control",
             [](atvv::Session& s, py::bytes data) -> py::dict {
                 const std::string_view view = data;
                 std::span<const std::uint8_t> bytes(
                     reinterpret_cast<const std::uint8_t*>(view.data()),
                     view.size());
                 auto event = s.handle_control(bytes);
                 py::dict out;
                 if (std::holds_alternative<atvv::CapsReceived>(event)) {
                     const auto& e = std::get<atvv::CapsReceived>(event);
                     out["opcode"] = "Caps";
                     out["capabilities"] = e.capabilities;
                 } else if (std::holds_alternative<
                                atvv::MicButtonPressed>(event)) {
                     out["opcode"] = "MicButton";
                 } else if (std::holds_alternative<atvv::AudioStarted>(
                                event)) {
                     const auto& e = std::get<atvv::AudioStarted>(event);
                     out["opcode"] = "AudioStart";
                     if (e.session_id.has_value()) {
                         out["session_id"] = py::int_(*e.session_id);
                     } else {
                         out["session_id"] = py::none();
                     }
                 } else if (std::holds_alternative<atvv::AudioStopped>(
                                event)) {
                     out["opcode"] = "AudioStop";
                 } else if (std::holds_alternative<atvv::AudioSynced>(
                                event)) {
                     out["opcode"] = "AudioSync";
                 } else {
                     const auto& e =
                         std::get<atvv::UnknownControl>(event);
                     out["opcode"] = "Unknown";
                     out["raw_opcode"] = e.opcode;
                 }
                 return out;
             },
             py::arg("payload"),
             "Dispatch a device->host control payload. Returns a\n"
             "dict with an 'opcode' key plus per-opcode fields\n"
             "matching the Phase 2 / Area 2 contract.")
        .def("handle_audio",
             [](atvv::Session& s, py::bytes data) -> std::vector<std::int16_t> {
                 const std::string_view view = data;
                 std::span<const std::uint8_t> bytes(
                     reinterpret_cast<const std::uint8_t*>(view.data()),
                     view.size());
                 return s.handle_audio(bytes);
             },
             py::arg("payload"),
             "Decode one audio notification. Returns [] while the\n"
             "mic is closed AND inside the late-audio guard; once\n"
             "the guard expires, audio samples flow through the\n"
             "PCM pipeline.")
        .def("mic_open_command",
             [](const atvv::Session& s) -> py::bytes {
                 const auto v = s.mic_open_command();
                 return py::bytes(
                     reinterpret_cast<const char*>(v.data()), v.size());
             },
             "Encode host->device MIC_OPEN command carrying the\n"
             "negotiated protocol version.")
        .def("mic_close_command",
             [](const atvv::Session& s) -> py::bytes {
                 const auto v = s.mic_close_command();
                 return py::bytes(
                     reinterpret_cast<const char*>(v.data()), v.size());
             },
             "Encode host->device MIC_CLOSE command carrying the\n"
             "negotiated protocol version + last-seen session_id.");

    // ------------------------------------------------------------------
    // 11. IAudioRoute / WasapiAudioRoute / FakeAudioRoute
    //     (Phase 4 / ADR-0014 §3, §4). IAudioRoute is exposed as a
    //     trampoline class with polymorphic start/write/drain/stop/close
    //     so Python can hold either a real WASAPI route or a fake
    //     recording double through the same surface. PcmFormat is a
    //     small POD value type matching
    //     apps/windows/rc003/src/ovb_rc003/audio_playback.py:SOURCE_SAMPLE_RATE_HZ.
    //
    //     No native compute is exposed at this seam yet: WASAPI's
    //     single-owner rule (plan §3 rule 5) means the C++ side owns
    //     the device handle, so Python can only drive the lifecycle
    //     (start/write/drain/stop/close). step 4 wires FakeAudioRoute
    //     into the shadow parity harness.
    // ------------------------------------------------------------------
    py::class_<PcmFormat>(m, "PcmFormat")
        .def(py::init<>(),
             "Construct a 16 kHz mono int16 PCM format (the BLE\n"
             "ATVV source rate).")
        .def(py::init<std::uint32_t, std::uint16_t, std::uint16_t>(),
             py::arg("sample_rate"),
             py::arg("channels"),
             py::arg("bits_per_sample"))
        .def_readwrite("sample_rate", &PcmFormat::sample_rate)
        .def_readwrite("channels", &PcmFormat::channels)
        .def_readwrite("bits_per_sample", &PcmFormat::bits_per_sample);

    py::class_<IAudioRoute, std::shared_ptr<IAudioRoute>>(
        m, "IAudioRoute")
        .def("start",
             [](IAudioRoute& self, const PcmFormat& fmt) {
                 return self.start(fmt);
             },
             py::arg("format"),
             "Open the underlying device for ``format``. Returns\n"
             "True on success, False on any failure (caller must\n"
             "fail-closed).")
        .def("write",
             [](IAudioRoute& self,
                std::vector<std::int16_t> samples) {
                 return self.write(
                     std::span<const std::int16_t>(samples));
             },
             py::arg("samples"),
             "Enqueue samples; non-blocking. Returns False if the\n"
             "route is stopped or never started. Internally\n"
             "drop-oldest via BoundedPcmQueue.")
        .def("drain",
             [](IAudioRoute& self, std::int64_t timeout_ms) {
                 self.drain(std::chrono::milliseconds(timeout_ms));
             },
             py::arg("timeout_ms") = 500,
             "Block up to timeout_ms waiting for the writer queue\n"
             "and device buffer to drain. Never throws. After\n"
             "drain(), write() may still succeed (queue empty,\n"
             "writer alive); stop() ends the writer.")
        .def("stop", &IAudioRoute::stop,
             "Tell the writer thread to exit after the current\n"
             "chunk. Idempotent. Does NOT release device handles;\n"
             "call close() for that.")
        .def("close", &IAudioRoute::close,
             "Release device handles. Idempotent. Implies stop().\n"
             "After close() write() must return False.");

    py::class_<audio::WasapiAudioRoute, IAudioRoute,
               std::shared_ptr<audio::WasapiAudioRoute>>(
        m, "WasapiAudioRoute")
        .def(py::init([](std::string endpoint_name,
                         std::string host_api_name) {
                 auto to_wide = [](const std::string& s) {
                     if (s.empty()) return std::wstring{};
                     const int needed =
                         MultiByteToWideChar(CP_UTF8, 0, s.data(),
                                             static_cast<int>(s.size()),
                                             nullptr, 0);
                     std::wstring out(static_cast<std::size_t>(needed),
                                      L'\0');
                     MultiByteToWideChar(
                         CP_UTF8, 0, s.data(),
                         static_cast<int>(s.size()), &out[0], needed);
                     return out;
                 };
                 return std::make_shared<audio::WasapiAudioRoute>(
                     to_wide(endpoint_name),
                     to_wide(host_api_name));
             }),
             py::arg("endpoint_name"),
             py::arg("host_api_name") = L"",
             "Construct a WASAPI output route bound to\n"
             "(endpoint_name, host_api_name). Windows-only;\n"
             "start() returns False on non-Windows or when the\n"
             "endpoint cannot be resolved.")
        .def("dropped_count",
             &audio::WasapiAudioRoute::dropped_count,
             "Monotonic counter of samples dropped due to queue\n"
             "overflow. Reset only at construction.")
        .def("write_error_count",
             &audio::WasapiAudioRoute::write_error_count,
             "Number of WASAPI GetBuffer/ReleaseBuffer failures\n"
             "observed by the writer jthread.")
        .def("writer_thread_alive",
             &audio::WasapiAudioRoute::writer_thread_alive,
             "True if the writer jthread is currently joinable\n"
             "(i.e. start() succeeded and close() has not run).")
        .def("current_format",
             &audio::WasapiAudioRoute::current_format,
             "The PcmFormat passed to the most recent successful\n"
             "start(); default-constructed otherwise.");

    py::class_<audio::FakeAudioRoute, IAudioRoute,
               std::shared_ptr<audio::FakeAudioRoute>>(
        m, "FakeAudioRoute")
        .def(py::init<>(),
             "Construct a recording test double. Always returns\n"
             "True from start(); writes append to an internal\n"
             "buffer that tests can inspect.")
        .def("recorded_samples",
             &audio::FakeAudioRoute::recorded_samples,
             "Number of int16 samples written since start().")
        .def("recorded_samples_list",
             [](const audio::FakeAudioRoute& self) {
                 std::vector<std::int16_t> copy = self.recorded_snapshot();
                 std::vector<std::int32_t> out;
                 out.reserve(copy.size());
                 for (auto s : copy) {
                     out.push_back(static_cast<std::int32_t>(s));
                 }
                 return out;
             },
             "Snapshot of all int16 samples recorded since start(),\n"
             "as a Python list of int. Used by the parity harness to\n"
             "compare against the python FakePlaybackSink's\n"
             "recorded_samples_list byte-for-byte.")
        .def("peak",
             [](const audio::FakeAudioRoute& self) {
                 return self.peak_abs();
             },
             "Peak absolute value across all recorded samples.\n"
             "Returns 0 if the buffer is empty.")
        .def("rms",
             [](const audio::FakeAudioRoute& self) {
                 return self.rms_value();
             },
             "Root-mean-square of all recorded samples as a float.\n"
             "Returns 0.0 if the buffer is empty.")
        .def("write_call_count",
             &audio::FakeAudioRoute::write_call_count,
             "Monotonic counter of write() invocations.")
        .def("started_count",
             &audio::FakeAudioRoute::started_count,
             "Monotonic counter of successful start() invocations.")
        .def("stopped_count",
             &audio::FakeAudioRoute::stopped_count,
             "Monotonic counter of stop() invocations.")
        .def("closed_count",
             &audio::FakeAudioRoute::closed_count,
             "Monotonic counter of close() invocations.")
        .def("dropped_count",
             &audio::FakeAudioRoute::dropped_count,
             "Number of write() calls rejected because the route\n"
             "was not started.")
        .def("last_format",
             &audio::FakeAudioRoute::last_format,
             "The PcmFormat from the most recent start().");

    // ------------------------------------------------------------------
    // 12. Upsample16kTo48k (Phase 4 / ADR-0014 §3.3 step 4)
    //
    // The 3-tap linear interpolation is a pure function whose
    // byte-exact equivalence with audio_playback.py:154-172 is the
    // G3 upsample parity requirement (ADR-0014 §6 step 4). Both
    // python and native sides are driven from the parity test with
    // identical input sequences and asserted byte-equal.
    //
    // UpsampleState is a small POD struct holding the carry-over
    // previous_sample + have_previous flag. The python side stores
    // the same state as attributes on a thin wrapper class so
    // scenarios that span multiple writes can be parity-tested.
    // ------------------------------------------------------------------
    py::class_<audio::UpsampleState>(
        m, "UpsampleState")
        .def(py::init<>(),
             "Default-constructed state: no previous sample yet;\n"
             "the first source[0] will produce (s0, s0, s0).")
        .def_readwrite("previous_sample",
                       &audio::UpsampleState::previous_sample)
        .def_readwrite("have_previous",
                       &audio::UpsampleState::have_previous);

    m.def("upsample_16k_to_48k",
          [](std::vector<std::int32_t> source,
             audio::UpsampleState& state) {
              std::vector<std::int16_t> in;
              in.reserve(source.size());
              for (auto v : source) {
                  if (v > 32767) v = 32767;
                  if (v < -32768) v = -32768;
                  in.push_back(static_cast<std::int16_t>(v));
              }
              std::vector<std::int16_t> out =
                  audio::upsample_16k_to_48k(
                      std::span<const std::int16_t>(in), state);
              std::vector<std::int32_t> py_out;
              py_out.reserve(out.size());
              for (auto s : out) {
                  py_out.push_back(static_cast<std::int32_t>(s));
              }
              return py_out;
          },
          py::arg("source"),
          py::arg("state"),
          "Three-tap linear interpolation: every source sample\n"
          "expands to (prev + round(delta/3), prev + round(2*delta/3),\n"
          "current), rounded to nearest int16 and clamped. The\n"
          "UpsampleState is mutated in place to track the carry-over\n"
          "previous sample so consecutive calls compose the same way\n"
          "the python baseline does across BLE notifications.");

    // ------------------------------------------------------------------
    // 13. Phase 5 / ADR-0015 step 3: input layer binding seam.
    //
    // Shape mirrors Phase 4 step 3 (b5c4d9b): interfaces as
    // std::shared_ptr trampolines, recording doubles + pure-logic
    // classes cross-platform, real Win32 adapters behind #ifdef _WIN32
    // (their .cpp files fail-closed to ``start() == false`` on
    // non-Windows per ADR-0015 §2; on Windows they open real handles).
    //
    // ``set_event_sink`` on IInputSource takes a C-style function
    // pointer + opaque user_data. pybind11 cannot marshal a Python
    // callable directly into that signature, so the Python bridge
    // shim (``_NativeInputSource.set_event_sink``) does a ``hasattr``
    // fallback: if the binding does not yet expose the native sink
    // callback, the shim stores the callable on the python side.
    // Phase 7 Application coordinator will own the source + sink
    // lifetime and add proper callback marshaling at that seam.
    // ------------------------------------------------------------------

    // 13.1 Enums (cross-platform).
    py::enum_<input::InputEvent::SourceKind>(m, "InputSourceKind")
        .value("RawInputKeyboard", input::InputEvent::SourceKind::RawInputKeyboard)
        .value("RawInputHid",      input::InputEvent::SourceKind::RawInputHid)
        .value("FridaHidTap",      input::InputEvent::SourceKind::FridaHidTap)
        .value("LowLevelHook",     input::InputEvent::SourceKind::LowLevelHook)
        .value("Synthetic",        input::InputEvent::SourceKind::Synthetic);
        // NOTE: export_values() is intentionally NOT used. The existing
        // ErrorCode / VoiceTriggerMode / etc. enums in this binding
        // also omit export_values(); using it here would put
        // InputSourceKind.{RawInputKeyboard,...} at module scope and
        // collide with later enum registrations (e.g. registering
        // InputEventKind.SystemAction as a value AND SystemAction as a
        // type at the same module scope). Callers must use
        // _C.InputSourceKind.RawInputKeyboard rather than
        // _C.RawInputKeyboard. Same convention as ErrorCode.None.

    py::enum_<input::InputEvent::EventKind>(m, "InputEventKind")
        .value("KeyDown",      input::InputEvent::EventKind::KeyDown)
        .value("KeyUp",        input::InputEvent::EventKind::KeyUp)
        .value("KeyCancel",    input::InputEvent::EventKind::KeyCancel)
        .value("SystemAction", input::InputEvent::EventKind::SystemAction);

    py::enum_<input::SystemAction>(m, "SystemAction")
        .value("VolumeUp",    input::SystemAction::VolumeUp)
        .value("VolumeDown",  input::SystemAction::VolumeDown)
        .value("VolumeMute",  input::SystemAction::VolumeMute)
        .value("ShowDesktop", input::SystemAction::ShowDesktop)
        .value("Escape",      input::SystemAction::Escape)
        .value("Return",      input::SystemAction::Return)
        .value("Backspace",   input::SystemAction::Backspace)
        .value("ContextMenu", input::SystemAction::ContextMenu)
        .value("AppSwitch",   input::SystemAction::AppSwitch)
        .value("CodexOpen",   input::SystemAction::CodexOpen);

    py::enum_<input::ButtonId>(m, "ButtonId")
        .value("Power",      input::ButtonId::Power)
        .value("ArrowUp",    input::ButtonId::ArrowUp)
        .value("ArrowDown",  input::ButtonId::ArrowDown)
        .value("ArrowLeft",  input::ButtonId::ArrowLeft)
        .value("ArrowRight", input::ButtonId::ArrowRight)
        .value("Ok",         input::ButtonId::Ok)
        .value("Back",       input::ButtonId::Back)
        .value("VolumeUp",   input::ButtonId::VolumeUp)
        .value("VolumeDown", input::ButtonId::VolumeDown)
        .value("Home",       input::ButtonId::Home)
        .value("Menu",       input::ButtonId::Menu)
        .value("Tv",         input::ButtonId::Tv)
        .value("Mic",        input::ButtonId::Mic)
        .value("VolumeMute", input::ButtonId::VolumeMute);

    py::enum_<input::ResolvedAction::Kind>(m, "ResolvedActionKind")
        .value("KeySequence",  input::ResolvedAction::Kind::KeySequence)
        .value("SystemAction", input::ResolvedAction::Kind::SystemAction)
        .value("Disabled",     input::ResolvedAction::Kind::Disabled);

    // 13.2 InputEvent POD (cross-platform). The timestamp is exposed
    // as nanoseconds-since-epoch so pybind11/chrono doesn't need to
    // special-case steady_clock on every consumer.
    py::class_<input::InputEvent>(m, "InputEvent")
        .def(py::init<>(),
             "Default-constructed event: SourceKind::RawInputKeyboard\n"
             "+ EventKind::KeyDown + vk/scan/usage/extra_info all 0.")
        .def_readwrite("source",     &input::InputEvent::source)
        .def_readwrite("kind",       &input::InputEvent::kind)
        .def_readwrite("vk_code",    &input::InputEvent::vk_code)
        .def_readwrite("scan_code",  &input::InputEvent::scan_code)
        .def_readwrite("usage_id",   &input::InputEvent::usage_id)
        .def_readwrite("extra_info", &input::InputEvent::extra_info)
        .def_readwrite("injected",   &input::InputEvent::injected)
        .def_readwrite("extended",   &input::InputEvent::extended);

    // 13.3 ResolvedAction POD (cross-platform).
    py::class_<input::ResolvedAction>(m, "ResolvedAction")
        .def(py::init<>(),
             "Default-constructed ResolvedAction: kind=Disabled,\n"
             "vk_code=0, system_action=Escape, key_down=true.")
        .def_readwrite("kind",           &input::ResolvedAction::kind)
        .def_readwrite("vk_code",        &input::ResolvedAction::vk_code)
        .def_readwrite("system_action",  &input::ResolvedAction::system_action)
        .def_readwrite("key_down",       &input::ResolvedAction::key_down);

    // 13.4 IInputSource interface + recording double (cross-platform).
    py::class_<input::IInputSource, std::shared_ptr<input::IInputSource>>(
        m, "IInputSource")
        .def("start",
             &input::IInputSource::start,
             "Open the underlying handle / pump thread. Returns\n"
             "True on success; False if already started or if the\n"
             "underlying transport refused. Single-owner: only one\n"
             "IInputSource may be in ``started == true`` state at any\n"
             "moment (plan §3 rule 5).")
        .def("stop",
             &input::IInputSource::stop,
             "Tell the source to exit its pump thread and release\n"
             "the underlying handle. Idempotent.")
        .def("event_count",
             &input::IInputSource::event_count,
             "Monotonic counter of InputEvents delivered to the\n"
             "registered sink since construction.")
        .def("dropped_count",
             &input::IInputSource::dropped_count,
             "Monotonic counter of events that the source dropped\n"
             "before delivery (typically SPSC ring overflow).");

    py::class_<input::FakeInputSource, input::IInputSource,
               std::shared_ptr<input::FakeInputSource>>(
        m, "FakeInputSource")
        .def(py::init<>(),
             "Cross-platform test double. Does NOT start a thread;\n"
             "events arrive via ``inject_event_for_test``.")
        .def("inject_event_for_test",
             &input::FakeInputSource::inject_event_for_test,
             py::arg("event"),
             "Test-only helper: deliver ``event`` to the registered\n"
             "sink as if it had come from a real pump thread.")
        .def("set_dropped_count_for_test",
             &input::FakeInputSource::set_dropped_count_for_test,
             py::arg("dropped"),
             "Test-only helper: override the dropped counter.")
        .def("recorded_events",
             &input::FakeInputSource::recorded_events,
             "Snapshot under mutex; safe to call from any thread.");

#ifdef _WIN32
    py::class_<input::RawInputSource, input::IInputSource,
               std::shared_ptr<input::RawInputSource>>(
        m, "RawInputSource")
        .def(py::init<>(),
             "Real Windows Raw Input adapter (RIDEV_INPUTSINK +\n"
             "RC003 VID/PID 0x2717/0x32B8 filter + RIM_TYPEKEYBOARD\n"
             "/RIM_TYPEHID decoding). Windows-only; on non-Windows\n"
             "the binding does not expose this class at all.");

    py::class_<input::LowLevelKeyboardHook, input::IInputSource,
               std::shared_ptr<input::LowLevelKeyboardHook>>(
        m, "LowLevelKeyboardHook")
        .def(py::init<>(),
             "Real WH_KEYBOARD_LL hook dispatcher (5 us budget\n"
             "per ADR-0015 §3.4). Windows-only.")
        .def("slow_callback_count",
             &input::LowLevelKeyboardHook::slow_callback_count,
             "Diagnostic: number of hook callbacks that exceeded the\n"
             "5 us QPC budget. Not used for control flow; surfaced\n"
             "in app.log when non-zero.");

    py::class_<input::FridaHidTapSource, input::IInputSource,
               std::shared_ptr<input::FridaHidTapSource>>(
        m, "FridaHidTapSource")
        .def(py::init<>(),
             "Real Frida IPC loopback socket reader\n"
             "(127.0.0.1:REMOTE_MIC_RC003_HID_TAP_PORT, default\n"
             "30684). Windows-only; returns False when no Frida\n"
             "Gadget is listening.");
#endif

    // 13.5 IHostActionSink interface + recording double (cross-platform).
    py::class_<input::IHostActionSink,
               std::shared_ptr<input::IHostActionSink>>(
        m, "IHostActionSink")
        .def("submit_key",
             [](input::IHostActionSink& self,
                std::uint16_t vk, bool down,
                std::chrono::milliseconds deadline) {
                 return self.submit_key(vk, down, deadline);
             },
             py::arg("vk_code"),
             py::arg("key_down"),
             py::arg("deadline_ms") = 50,
             "Submit a single VK key down/up to the host. The\n"
             "binding converts ``deadline_ms`` from int to\n"
             "std::chrono::milliseconds. Returns False if the sink\n"
             "is not started or if the underlying transport refused.")
        .def("submit_system_action",
             &input::IHostActionSink::submit_system_action,
             py::arg("action"),
             "Submit a semantic system action (volume / showDesktop\n"
             "/ openCodex / etc.). Returns False on sink-down.")
        .def("cancel_pending",
             &input::IHostActionSink::cancel_pending,
             "Drop any pending key events from the worker queue.")
        .def("start", &input::IHostActionSink::start,
             "Start the worker thread / dispatch handle. Idempotent.")
        .def("stop", &input::IHostActionSink::stop,
             "Stop the worker thread and release the underlying\n"
             "handle. Idempotent.")
        .def("submit_error_count",
             &input::IHostActionSink::submit_error_count,
             "Monotonic counter of submit_* calls that returned\n"
             "False or raised.")
        .def("submitted_count",
             &input::IHostActionSink::submitted_count,
             "Monotonic counter of submit_* calls that succeeded.");

    py::class_<input::FakeHostActionSink, input::IHostActionSink,
               std::shared_ptr<input::FakeHostActionSink>>(
        m, "FakeHostActionSink")
        .def(py::init<>(),
             "Cross-platform test double. Records every submit_key\n"
             "/ submit_system_action under a mutex; no side effects\n"
             "to the host keyboard / shell.")
        .def("recorded_keys",
             &input::FakeHostActionSink::recorded_keys,
             "Snapshot of (vk, key_down) pairs recorded since\n"
             "start(); safe to call from any thread.")
        .def("recorded_system_actions",
             &input::FakeHostActionSink::recorded_system_actions,
             "Snapshot of SystemAction values recorded since\n"
             "start(); safe to call from any thread.")
        .def("pending_count",
             &input::FakeHostActionSink::pending_count,
             "Number of submit_key calls still pending in the\n"
             "internal queue (FakeHostActionSink is a sync\n"
             "recording double so this stays at 0).")
        .def("set_submit_fails_for_test",
             &input::FakeHostActionSink::set_submit_fails_for_test,
             py::arg("fails"),
             "Test-only helper: configure the sink to reject every\n"
             "submit_* call (returns false).");

#ifdef _WIN32
    py::class_<input::SendInputActionSink, input::IHostActionSink,
               std::shared_ptr<input::SendInputActionSink>>(
        m, "SendInputActionSink")
        .def(py::init<>(),
             "Real SendInput adapter (bounded queue + worker thread\n"
             "+ physical scan-code modifiers per ADR-0015 §3.7).\n"
             "Windows-only; on non-Windows the binding does not\n"
             "expose this class at all.");
#endif

    // 13.6 ActionResolver + DefaultActionResolver (cross-platform).
    py::class_<input::ActionResolver, std::shared_ptr<input::ActionResolver>>(
        m, "ActionResolver")
        .def("resolve",
             [](const input::ActionResolver& self, input::ButtonId btn)
                 -> std::optional<input::ResolvedAction> {
                 return self.resolve(btn);
             },
             py::arg("button"),
             "Resolve a ButtonId to a concrete ResolvedAction.\n"
             "Returns None when the button is unbound in the default\n"
             "table AND no user override is provided.");

    py::class_<input::DefaultActionResolver, input::ActionResolver,
               std::shared_ptr<input::DefaultActionResolver>>(
        m, "DefaultActionResolver")
        .def(py::init<>(),
             "Real default-table resolver mirroring\n"
             "apps/windows/rc003/src/ovb_rc003/key_mapping.py:104-117\n"
             "byte-identical for VK codes (G3 byte-exact parity per\n"
             "ADR-0015 §10 step 3). Pure-logic; thread-safe.");

    // 13.7 HotkeyPhysicalizer (cross-platform; depends on IHostActionSink).
    py::class_<input::HotkeyPhysicalizer>(
        m, "HotkeyPhysicalizer",
        py::keep_alive<1, 2>())
        .def(py::init<input::IHostActionSink&>(),
             py::arg("sink"),
             "Construct a physicalizer bound to ``sink``. The\n"
             "physicalizer holds a reference to ``sink`` (not\n"
             "ownership); py::keep_alive pins the lifetime so the\n"
             "physicalizer is destroyed before its sink.")
        .def("physicalize",
             [](input::HotkeyPhysicalizer& self,
                const char* tokens) {
                 return self.physicalize(tokens);
             },
             py::arg("tokens"),
             "Resolve ``tokens`` (slash-separated chord names, e.g.\n"
             "\"lctrl+lalt\") and submit the resulting VK sequence\n"
             "through the bound sink. Returns False on unknown\n"
             "token or sink-down.")
        .def("release_held",
             &input::HotkeyPhysicalizer::release_held,
             "Best-effort cleanup: re-release any keys the\n"
             "physicalizer currently holds down. Step 2 sub-pass A\n"
             "leaves this as a no-op; Phase 7 wires the real release\n"
             "surface through SendInputActionSink.");
}