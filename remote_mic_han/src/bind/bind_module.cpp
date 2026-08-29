#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include "remotemic/bind/errors.hpp"
#include "remotemic/bind/probe_types.hpp"
#include "remotemic/atvv/capabilities.hpp"

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
}