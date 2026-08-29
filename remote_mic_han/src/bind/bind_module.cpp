#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include "remotemic/bind/errors.hpp"
#include "remotemic/bind/probe_types.hpp"
#include "remotemic/atvv/capabilities.hpp"
#include "remotemic/atvv/control.hpp"
#include "remotemic/adpcm/ima_decoder.hpp"
#include "remotemic/adpcm/dc_highpass.hpp"
#include "remotemic/adpcm/postprocess.hpp"
#include "remotemic/adpcm/frame_accumulator.hpp"

#include <variant>

namespace py = pybind11;
using namespace remotemic;

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
}