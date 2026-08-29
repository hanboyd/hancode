// Phase 2 / Area 1: STUB implementation of ATVV capability parse.
//
// This is the TDD red state at the end of step 2: parse() always
// returns std::nullopt regardless of input. The unit test in
// tests/unit/test_atvv_capabilities.cpp loads the JSON golden
// fixtures and expects:
//
//   - the three reject fixtures (synthetic-legacy-rejects-short.json,
//     synthetic-wrong-opcode.json, synthetic-short-payload.json) to
//     pass (parse() == nullopt matches their `expected: null`)
//   - the five valid fixtures to FAIL because parse() returns
//     nullopt but they expect a populated Capabilities struct.
//
// Step 3 replaces this body with the real parser matching the
// Python ATVVCapabilities.parse contract byte-for-byte; see
// apps/windows/rc003/src/ovb_rc003/atvv_protocol.py:73-101 and
// ADR-0012 §3.

#include "remotemic/atvv/capabilities.hpp"

namespace remotemic::atvv {

std::optional<Capabilities> parse(std::span<const std::uint8_t>) noexcept {
    return std::nullopt;
}

}  // namespace remotemic::atvv