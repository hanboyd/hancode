cmake_minimum_required(VERSION 3.25)

project(RemoteMicWindows VERSION 1.0.0 LANGUAGES CXX)

option(REMOTEMIC_BUILD_TESTS "Build hardware-free C++ tests" ON)
option(REMOTEMIC_BUILD_PYTHON "Build the CPython binding extension" OFF)
option(REMOTEMIC_PYBIND11_FETCH "Fetch pybind11 via FetchContent (else find_package)" ON)

# Resolve the project's pinned CPython 3.11 venv interpreter BEFORE any
# ``add_test(... ${Python3_EXECUTABLE} ...)`` call. CMake evaluates
# ``${Python3_EXECUTABLE}`` at configure time when ``add_test`` is
# processed; a ``find_package(Python3 ...)`` after the test
# registrations leaves the variable empty in the generated
# CTestTestfile.cmake (manifested as ``-m`` being treated as the
# executable and ctest failing with "Could not find executable -m").
if(REMOTEMIC_BUILD_PYTHON)
    set(Python3_EXECUTABLE
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/.venv/Scripts/python.exe"
        CACHE FILEPATH "CPython 3.11 interpreter from the project venv"
    )
    # Pin pybind11's internal uppercase view to the venv shim. The shim
    # calls into the uv-installed CPython 3.11 base; pybind11 queries it
    # via sysconfig and discovers the correct include/lib paths and ABI
    # tag (.cp311-win_amd64.pyd). Without this override pybind11 picks the
    # first Python on PATH (here, mise-installed 3.14) and stamps cp314.
    set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "" FORCE)
    set(Python_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "" FORCE)
    find_package(Python3 3.11 COMPONENTS Interpreter Development REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Pure core: protocol/session/queue/config types only.
# No <fstream>, no <windows.h>, no env-var reads, no filesystem writes.
# Probe types and the Error category live here as the smallest reasonable
# set of pure-logic objects the smoke test exercises.
# ---------------------------------------------------------------------------
add_library(remotemic_core STATIC
    src/bind/probe_types.cpp
    src/bind/errors.cpp
)

target_include_directories(remotemic_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(remotemic_core PUBLIC cxx_std_20)
target_compile_definitions(remotemic_core
    PUBLIC
        REMOTEMIC_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(remotemic_core PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# ATVV / ADPCM pure logic (Phase 2, per ADR-0012).
# No <windows.h>, no <fstream>, no env-var reads, no filesystem writes.
# Header-only contracts live under include/remotemic/atvv/ and
# include/remotemic/adpcm/; implementations under src/atvv/ and
# src/adpcm/. nlohmann::json is the ONLY C++ dependency added in
# Phase 2 (used by tests to read the golden JSON fixtures shared
# with the Python side); see ADR-0012 §4 / §7.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(remotemic_atvv STATIC
    src/atvv/capabilities.cpp
    src/atvv/control.cpp
)

target_include_directories(remotemic_atvv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_atvv
    PUBLIC nlohmann_json::nlohmann_json
)

target_compile_features(remotemic_atvv PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_atvv PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_atvv PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# IMA/DVI ADPCM decoder + DC high-pass + smoothing/gain + FrameAccumulator
# (Phase 2 / Area 3 + Area 4, per ADR-0012). Same constraints as
# remotemic_atvv above: pure logic, no <windows.h>, no <fstream>, no env-var
# reads, no filesystem writes. nlohmann_json is inherited via the
# transitive dependency on remotemic_atvv (only Area 3 tests need it; the
# decoder itself has no JSON dependency).
# ---------------------------------------------------------------------------
add_library(remotemic_adpcm STATIC
    src/adpcm/ima_decoder.cpp
    src/adpcm/dc_highpass.cpp
    src/adpcm/postprocess.cpp
    src/adpcm/frame_accumulator.cpp
)

target_include_directories(remotemic_adpcm
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_adpcm PUBLIC remotemic_atvv)

target_compile_features(remotemic_adpcm PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_adpcm PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_adpcm PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Win32 platform layer: filesystem, env, logger, Application::diagnose.
# May include <windows.h> and other OS headers. Consumes remotemic_core.
# ---------------------------------------------------------------------------
add_library(remotemic_platform_win32 STATIC
    src/app/application.cpp
    src/logging/file_logger.cpp
    src/platform/runtime_paths.cpp
)

target_link_libraries(remotemic_platform_win32 PUBLIC remotemic_core)

# ---------------------------------------------------------------------------
# Phase 3 / ADR-0013: voice-edge pure logic.
# Pure state machine + injectable timer; no <windows.h>, no I/O, no
# threads of its own. Step 1 (this commit) ships the stub; step 2
# fills in the real state machine.
# ---------------------------------------------------------------------------
add_library(remotemic_voice STATIC
    src/voice/voice_controller.cpp
    src/voice/edge_debouncer.cpp
)

target_include_directories(remotemic_voice
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_voice PUBLIC remotemic_core)

target_compile_features(remotemic_voice PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_voice PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_voice PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Phase 3 / ADR-0013: ATVV session state machine.
# Pure value-class over the existing Phase 2 atvv + adpcm types.
# No <windows.h>, no I/O, no threads of its own. Depends on both
# ``remotemic_atvv`` (Capabilities + control parser) and
# ``remotemic_adpcm`` (decoder + DC filter + postprocess +
# FrameAccumulator); the dependency direction avoids a library cycle
# because ``remotemic_adpcm`` does NOT need to know about Session.
# ---------------------------------------------------------------------------
add_library(remotemic_session STATIC
    src/atvv/session.cpp
)

target_include_directories(remotemic_session
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_session
    PUBLIC
        remotemic_atvv
        remotemic_adpcm
)

target_compile_features(remotemic_session PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_session PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_session PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Phase 4 / ADR-0014 step 1: bounded PCM queue + WASAPI AudioRoute
# (stub). Pure C++ queue / chunker / upsampler are hardware-free;
# WasapiAudioRoute stub returns false from start() to keep Windows-only
# headers out of the test build until step 2 lands the real WASAPI
# implementation. FakeAudioRoute is a recording double used by the
# shadow parity tests (step 4) and by Linux/macOS CI.
# ---------------------------------------------------------------------------
add_library(remotemic_audio STATIC
    src/audio/bounded_pcm_queue.cpp
    src/audio/pcm_chunker.cpp
    src/audio/upsample_16k_to_48k.cpp
    src/audio/fake_audio_route.cpp
    src/audio/wasapi_audio_route.cpp
)

# ---------------------------------------------------------------------------
# Phase 5 / ADR-0015 step 1: Windows input + host action sink
# (stub). Pure C++ interfaces + recording doubles + Windows-only
# stub implementations (refuse start()). Step 2 replaces each stub
# with a real Win32 adapter. The recording doubles are cross-OS
# (no <windows.h>) so tests compile on Linux/macOS CI runners.
# ---------------------------------------------------------------------------
add_library(remotemic_input STATIC
    src/input/action_resolver.cpp
    src/input/fake_input_source.cpp
    src/input/fake_host_action_sink.cpp
    src/input/hotkey_physicalizer.cpp
    src/input/low_level_keyboard_hook.cpp
    src/input/raw_input_source.cpp
    src/input/frida_hid_tap_source.cpp
    src/input/send_input_action_sink.cpp
)

add_library(remotemic_app STATIC
    src/app/application_coordinator.cpp
    src/audio/windows_voice_audio_policy_lease.cpp
)
target_include_directories(remotemic_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(remotemic_app PUBLIC cxx_std_20)
if(WIN32)
    target_link_libraries(remotemic_app PRIVATE ole32 propsys advapi32)
endif()

# Phase 9: Qt-independent state behind the unchanged PySide6/QML client.
add_library(remotemic_ui STATIC
    src/ui/settings_state.cpp
)
target_include_directories(remotemic_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(remotemic_ui PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(remotemic_ui PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_ui PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Phase 6: single-owner BLE/WinRT transport plus a hardware-free recording
# double. Python keeps the already-validated enumeration/identity policy;
# the selected device's GATT connection and callbacks have one native owner.
add_library(remotemic_ble STATIC
    src/ble/fake_ble_transport.cpp
    src/ble/winrt_ble_transport.cpp
)

target_include_directories(remotemic_ble
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(remotemic_ble PUBLIC remotemic_core)
if(WIN32)
    target_link_libraries(remotemic_ble PRIVATE windowsapp)
endif()
target_compile_features(remotemic_ble PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(remotemic_ble PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_ble PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(remotemic_input
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_input PUBLIC remotemic_core)

target_compile_features(remotemic_input PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_input PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_input PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(remotemic_audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(remotemic_audio PUBLIC remotemic_core)

target_compile_features(remotemic_audio PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(remotemic_audio PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_audio PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MSVC)
    target_compile_options(remotemic_platform_win32 PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic_platform_win32 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# CLI executable stays platform-aware but free of pybind11.
# ---------------------------------------------------------------------------
add_executable(remotemic apps/cli/main.cpp)
target_link_libraries(remotemic PRIVATE remotemic_platform_win32)

if(MSVC)
    target_compile_options(remotemic PRIVATE /W4 /permissive- /EHsc /utf-8)
else()
    target_compile_options(remotemic PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS remotemic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ---------------------------------------------------------------------------
# Hardware-free C++ tests. Link the platform layer because they exercise
# Application, RuntimePaths and the default PCM format.
# ---------------------------------------------------------------------------
if(REMOTEMIC_BUILD_TESTS)
    enable_testing()

    add_executable(remotemic_unit_tests tests/unit/core_tests.cpp)
    target_link_libraries(remotemic_unit_tests PRIVATE remotemic_platform_win32)
    if(MSVC)
        target_compile_options(remotemic_unit_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_unit_tests COMMAND remotemic_unit_tests)

    # Phase 2 / Area 1 TDD red state: stub returns std::nullopt, so
    # the three reject fixtures pass and the five valid fixtures
    # fail. This is intentional; step 3 implements the real parser.
    # See ADR-0012 §3 / §8 and tests/unit/test_atvv_capabilities.cpp.
    add_executable(remotemic_atvv_tests tests/unit/test_atvv_capabilities.cpp)
    target_link_libraries(remotemic_atvv_tests PRIVATE remotemic_atvv)
    if(MSVC)
        target_compile_options(remotemic_atvv_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_atvv_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_atvv_tests COMMAND remotemic_atvv_tests)
    # The JSON fixtures live in the source tree; the test walks up
    # from CTest's WORKING_DIRECTORY (= build dir) to find them. Setting
    # an explicit WORKING_DIRECTORY at the source root keeps the path
    # lookup short and deterministic.
    set_tests_properties(remotemic_atvv_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 2 / Area 2 TDD red state: stub returns std::nullopt / empty
    # vector, so the empty-input decode fixture passes and the 4 encode
    # + 7 valid decode fixtures fail. See ADR-0012 §3 / §8 and
    # tests/unit/test_atvv_control.cpp.
    add_executable(remotemic_atvv_control_tests tests/unit/test_atvv_control.cpp)
    target_link_libraries(remotemic_atvv_control_tests PRIVATE remotemic_atvv)
    if(MSVC)
        target_compile_options(remotemic_atvv_control_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_atvv_control_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_atvv_control_tests COMMAND remotemic_atvv_control_tests)
    set_tests_properties(remotemic_atvv_control_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 2 / Area 3 TDD red state: stub returns {} for any input
    # and ignores reset(), so all 11 decode fixtures FAIL. This is
    # intentional; step 3 implements the real decoder. See
    # ADR-0012 section 3 / section 8 and tests/unit/test_adpcm_ima.cpp.
    add_executable(remotemic_adpcm_ima_tests tests/unit/test_adpcm_ima.cpp)
    target_link_libraries(remotemic_adpcm_ima_tests PRIVATE remotemic_adpcm)
    if(MSVC)
        target_compile_options(remotemic_adpcm_ima_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_adpcm_ima_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_adpcm_ima_tests COMMAND remotemic_adpcm_ima_tests)
    set_tests_properties(remotemic_adpcm_ima_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 2 / Area 4 TDD red state: stubs return {} for any input,
    # so all 5 dc-*.json + 10 postprocess-*.json + 7 frame-*.json
    # fixtures FAIL. See ADR-0012 section 3 / section 8 and
    # tests/unit/test_adpcm_{dc_highpass,postprocess,frame_accumulator}.cpp.
    add_executable(remotemic_adpcm_dc_tests tests/unit/test_adpcm_dc_highpass.cpp)
    target_link_libraries(remotemic_adpcm_dc_tests PRIVATE remotemic_adpcm)
    if(MSVC)
        target_compile_options(remotemic_adpcm_dc_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_adpcm_dc_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_adpcm_dc_tests COMMAND remotemic_adpcm_dc_tests)
    set_tests_properties(remotemic_adpcm_dc_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_adpcm_postprocess_tests tests/unit/test_adpcm_postprocess.cpp)
    target_link_libraries(remotemic_adpcm_postprocess_tests PRIVATE remotemic_adpcm)
    if(MSVC)
        target_compile_options(remotemic_adpcm_postprocess_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_adpcm_postprocess_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_adpcm_postprocess_tests COMMAND remotemic_adpcm_postprocess_tests)
    set_tests_properties(remotemic_adpcm_postprocess_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_adpcm_frame_tests tests/unit/test_adpcm_frame_accumulator.cpp)
    target_link_libraries(remotemic_adpcm_frame_tests PRIVATE remotemic_adpcm)
    if(MSVC)
        target_compile_options(remotemic_adpcm_frame_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_adpcm_frame_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_adpcm_frame_tests COMMAND remotemic_adpcm_frame_tests)
    set_tests_properties(remotemic_adpcm_frame_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 3 / ADR-0013 step 1: VoiceController TDD red-state
    # tests. Stub returns the no-op default for every event so the
    # stateful paths fail; step 2 turns them green.
    add_executable(remotemic_voice_controller_tests tests/unit/test_voice_controller.cpp)
    target_link_libraries(remotemic_voice_controller_tests PRIVATE remotemic_voice)
    if(MSVC)
        target_compile_options(remotemic_voice_controller_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_voice_controller_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_voice_controller_tests COMMAND remotemic_voice_controller_tests)
    set_tests_properties(remotemic_voice_controller_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 3 / ADR-0013 step 1: VoiceEdgeDebouncer TDD red-state
    # tests with a manual timer factory so no thread / sleep is used.
    add_executable(remotemic_edge_debouncer_tests tests/unit/test_edge_debouncer.cpp)
    target_link_libraries(remotemic_edge_debouncer_tests PRIVATE remotemic_voice)
    if(MSVC)
        target_compile_options(remotemic_edge_debouncer_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_edge_debouncer_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_edge_debouncer_tests COMMAND remotemic_edge_debouncer_tests)
    set_tests_properties(remotemic_edge_debouncer_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 3 / ADR-0013 step 1: ATVV Session TDD red-state tests
    # driving ``Session::handle_control`` / ``handle_audio`` with
    # constructed payloads and a ManualClock so the late-audio guard
    # never depends on real wall-clock time.
    add_executable(remotemic_session_tests tests/unit/test_atvv_session.cpp)
    target_link_libraries(remotemic_session_tests PRIVATE remotemic_session)
    if(MSVC)
        target_compile_options(remotemic_session_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_session_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_session_tests COMMAND remotemic_session_tests)
    set_tests_properties(remotemic_session_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 4 / ADR-0014 step 1: audio modules TDD red-state tests.
    # Stub behavior makes every contract assertion FAIL on purpose; step
    # 2 turns them green. See ADR-0014 section 8 (G1).
    add_executable(remotemic_bounded_pcm_queue_tests tests/unit/test_bounded_pcm_queue.cpp)
    target_link_libraries(remotemic_bounded_pcm_queue_tests PRIVATE remotemic_audio)
    if(MSVC)
        target_compile_options(remotemic_bounded_pcm_queue_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_bounded_pcm_queue_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_bounded_pcm_queue_tests COMMAND remotemic_bounded_pcm_queue_tests)
    set_tests_properties(remotemic_bounded_pcm_queue_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_pcm_chunker_tests tests/unit/test_pcm_chunker.cpp)
    target_link_libraries(remotemic_pcm_chunker_tests PRIVATE remotemic_audio)
    if(MSVC)
        target_compile_options(remotemic_pcm_chunker_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_pcm_chunker_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_pcm_chunker_tests COMMAND remotemic_pcm_chunker_tests)
    set_tests_properties(remotemic_pcm_chunker_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_upsample_tests tests/unit/test_upsample_16k_to_48k.cpp)
    target_link_libraries(remotemic_upsample_tests PRIVATE remotemic_audio)
    if(MSVC)
        target_compile_options(remotemic_upsample_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_upsample_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_upsample_tests COMMAND remotemic_upsample_tests)
    set_tests_properties(remotemic_upsample_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_fake_audio_route_tests tests/unit/test_fake_audio_route.cpp)
    target_link_libraries(remotemic_fake_audio_route_tests PRIVATE remotemic_audio)
    if(MSVC)
        target_compile_options(remotemic_fake_audio_route_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_fake_audio_route_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_fake_audio_route_tests COMMAND remotemic_fake_audio_route_tests)
    set_tests_properties(remotemic_fake_audio_route_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_wasapi_audio_route_tests tests/unit/test_wasapi_audio_route.cpp)
    target_link_libraries(remotemic_wasapi_audio_route_tests PRIVATE remotemic_audio)
    if(MSVC)
        target_compile_options(remotemic_wasapi_audio_route_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_wasapi_audio_route_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_wasapi_audio_route_tests COMMAND remotemic_wasapi_audio_route_tests)
    set_tests_properties(remotemic_wasapi_audio_route_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 1: input value-type round-trip + enum +
    # invariants. Pure cross-OS test; compiles on Linux/macOS CI.
    add_executable(remotemic_input_event_tests tests/unit/test_input_event.cpp)
    target_link_libraries(remotemic_input_event_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_input_event_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_input_event_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_input_event_tests COMMAND remotemic_input_event_tests)
    set_tests_properties(remotemic_input_event_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 1: IInputSource + recording double +
    # stub red-state tests. Asserts Windows-only stubs refuse start()
    # until step 2 lands.
    add_executable(remotemic_i_input_source_tests tests/unit/test_i_input_source.cpp)
    target_link_libraries(remotemic_i_input_source_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_i_input_source_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_i_input_source_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_i_input_source_tests COMMAND remotemic_i_input_source_tests)
    set_tests_properties(remotemic_i_input_source_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 1: IHostActionSink + recording double +
    # stub red-state tests.
    add_executable(remotemic_i_host_action_sink_tests tests/unit/test_i_host_action_sink.cpp)
    target_link_libraries(remotemic_i_host_action_sink_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_i_host_action_sink_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_i_host_action_sink_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_i_host_action_sink_tests COMMAND remotemic_i_host_action_sink_tests)
    set_tests_properties(remotemic_i_host_action_sink_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 1: LowLevelKeyboardHook stub red-state
    # tests (asserts start() returns false until step 2 lands the real
    # WH_KEYBOARD_LL implementation).
    add_executable(remotemic_low_level_hook_stub_tests tests/unit/test_low_level_keyboard_hook_stub.cpp)
    target_link_libraries(remotemic_low_level_hook_stub_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_low_level_hook_stub_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_low_level_hook_stub_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_low_level_hook_stub_tests COMMAND remotemic_low_level_hook_stub_tests)
    set_tests_properties(remotemic_low_level_hook_stub_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 2: real default-table ActionResolver tests
    # (mirror key_mapping.py:104-117). The step 1 stub is removed; the
    # step 1 red-state row is preserved as the comment below.
    add_executable(remotemic_action_resolver_tests tests/unit/test_action_resolver.cpp)
    target_link_libraries(remotemic_action_resolver_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_action_resolver_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_action_resolver_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_action_resolver_tests COMMAND remotemic_action_resolver_tests)
    set_tests_properties(remotemic_action_resolver_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    # Phase 5 / ADR-0015 step 2: HotkeyPhysicalizer chord-name parser +
    # VK code table tests (mirror hotkey.py + win32_keys.py).
    add_executable(remotemic_hotkey_physicalizer_tests tests/unit/test_hotkey_physicalizer.cpp)
    target_link_libraries(remotemic_hotkey_physicalizer_tests PRIVATE remotemic_input)
    if(MSVC)
        target_compile_options(remotemic_hotkey_physicalizer_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_hotkey_physicalizer_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_hotkey_physicalizer_tests COMMAND remotemic_hotkey_physicalizer_tests)
    set_tests_properties(remotemic_hotkey_physicalizer_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(remotemic_fake_ble_transport_tests tests/unit/test_fake_ble_transport.cpp)
    target_link_libraries(remotemic_fake_ble_transport_tests PRIVATE remotemic_ble)
    if(MSVC)
        target_compile_options(remotemic_fake_ble_transport_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_fake_ble_transport_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_fake_ble_transport_tests COMMAND remotemic_fake_ble_transport_tests)
    set_tests_properties(remotemic_fake_ble_transport_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        LABELS "phase6;ble;unit"
    )

    add_executable(remotemic_application_coordinator_tests
        tests/unit/test_application_coordinator.cpp)
    target_link_libraries(remotemic_application_coordinator_tests PRIVATE
        remotemic_app remotemic_session remotemic_voice remotemic_audio
        remotemic_input remotemic_ble)
    if(MSVC)
        target_compile_options(remotemic_application_coordinator_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_application_coordinator_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_application_coordinator_tests
        COMMAND remotemic_application_coordinator_tests)

    add_executable(remotemic_ui_settings_state_tests
        tests/unit/test_ui_settings_state.cpp)
    target_link_libraries(remotemic_ui_settings_state_tests PRIVATE remotemic_ui)
    if(MSVC)
        target_compile_options(remotemic_ui_settings_state_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
    else()
        target_compile_options(remotemic_ui_settings_state_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME remotemic_ui_settings_state_tests
        COMMAND remotemic_ui_settings_state_tests)
    set_tests_properties(remotemic_ui_settings_state_tests PROPERTIES
        LABELS "phase9;ui;unit")

    if(REMOTEMIC_BUILD_PYTHON)
        add_test(
            NAME remotemic_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -v
        )
        set_tests_properties(remotemic_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 2 / Area 1 step 4: ATVV binding smoke (G3 of ADR-0012).
        # Loads the bundled .pyd, calls atvv_capabilities_parse against
        # one valid + one reject JSON fixture, and asserts the return
        # shape matches the C++ unit test. Validation gate G3 of
        # ADR-0012 - on fail, do not flip the ADR to accepted.
        add_test(
            NAME remotemic_atvv_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_atvv_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_atvv_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 2 / Area 2 step 4: ATVV control binding smoke (G3 of
        # ADR-0012). Loads the bundled .pyd, calls atvv_control_parse /
        # atvv_mic_open_command / atvv_mic_close_command against each
        # control JSON fixture (4 encode + 8 decode) and asserts the
        # binding returns match the C++ unit test
        # (remotemic_atvv_control_tests). On fail, do not flip the ADR
        # to accepted.
        add_test(
            NAME remotemic_atvv_control_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_atvv_control_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_atvv_control_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 2 / Area 3 step 4: IMA/DVI ADPCM decoder binding smoke
        # (G3 of ADR-0012). Loads the bundled .pyd, creates an
        # ImaDecoder instance per fixture, calls reset + decode against
        # each adpcm-*.json fixture, and asserts the returned samples
        # match the C++ unit test (remotemic_adpcm_ima_tests). On
        # fail, do not flip the ADR to accepted.
        add_test(
            NAME remotemic_adpcm_ima_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_adpcm_ima_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_adpcm_ima_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 2 / Area 4 step 4: DC + postprocess + FrameAccumulator
        # binding smokes (G3 of ADR-0012). Each loads the bundled .pyd,
        # creates a fresh instance / calls the free function per
        # fixture, and asserts the returned values match the C++ unit
        # tests (remotemic_adpcm_dc_tests / _postprocess_tests /
        # _frame_tests). On fail, do not flip the ADR to accepted.
        add_test(
            NAME remotemic_adpcm_dc_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_adpcm_dc_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_adpcm_dc_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        add_test(
            NAME remotemic_adpcm_postprocess_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_adpcm_postprocess_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_adpcm_postprocess_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        add_test(
            NAME remotemic_adpcm_frame_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_adpcm_frame_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_adpcm_frame_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 3 / ADR-0013 step 3: voice + ATVV session binding
        # smokes. Each loads the bundled .pyd, creates a fresh
        # instance, and asserts the binding surface / returned dict
        # shape matches the Python baseline (apps/windows/rc003/src/
        # ovb_rc003/voice_controller.py, voice_edge_debouncer.py,
        # atvv_session.py).
        add_test(
            NAME remotemic_voice_controller_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_voice_controller_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_voice_controller_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        add_test(
            NAME remotemic_voice_edge_debouncer_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_voice_edge_debouncer_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_voice_edge_debouncer_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        add_test(
            NAME remotemic_atvv_session_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_atvv_session_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_atvv_session_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 4 / ADR-0014 §6 / step 3: IAudioRoute / WasapiAudioRoute
        # / FakeAudioRoute binding smoke (G3). Loads the bundled .pyd,
        # exercises the IAudioRoute lifecycle through FakeAudioRoute
        # (no real WASAPI device required), and asserts that the
        # PcmFormat value-type round-trips correctly. Real-device
        # validation lives in G6 (real RC003 + Typeless). On fail, do
        # not advance step 4 (shadow parity harness).
        add_test(
            NAME remotemic_audio_route_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_audio_route_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_audio_route_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        # Phase 5 / ADR-0015 §6 / step 3 closure: input-layer binding
        # smoke (G3). Loads the bundled .pyd, exercises the IInputSource
        # / IHostActionSink / HotkeyPhysicalizer lifecycle through the
        # recording doubles, and asserts the new set_event_sink C
        # function-pointer -> Python callback marshaling fires end-to-
        # end. Win32-only symbols (RawInputSource / SendInputActionSink)
        # are NOT exercised here; they fail-closed on non-Windows and
        # require real hardware for G6. On fail, do not advance the
        # step 3 closeout.
        add_test(
            NAME remotemic_input_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_input_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_input_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        )

        add_test(
            NAME remotemic_ble_transport_bind_smoke
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_ble_transport_bind_smoke.py"
                -v
        )
        set_tests_properties(remotemic_ble_transport_bind_smoke PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
            LABELS "phase6;ble;binding"
        )

        # Phase 3 / ADR-0013 step 4: runtime shadow parity tests
        # (G5). Each runs identical scripts through both the python
        # baseline and the C++ binding (via the bridge wrappers) and
        # asserts byte-exact equality. Env-leak safety: the env
        # override lives inside setUpClass with proper teardown
        # restore, matching the corrective fix pattern from
        # commit 5ce9bd5.
        # Helper script that bootstraps PYTHONPATH with both the build dir
        # (containing the compiled remotemic_native package) and the
        # source dir (containing ovb_rc003) before invoking unittest.
        # CMake's ENVIRONMENT property treats ``;`` as a list
        # separator and silently drops subsequent entries; this
        # wrapper sidesteps that by setting sys.path inside Python.
        set(_REMOTEMIC_PARITY_HELPER
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_parity_test.py"
        )
        set(_REMOTEMIC_PARITY_ENV
            "REMOTEMIC_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
            "REMOTEMIC_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/src"
        )

        # Phase 4 / ADR-0014 §6 / step 4: upsample byte-exact parity
        # (G3). Python 3-tap linear interpolation
        # (audio_playback.py:154-172) vs C++ Upsample16kTo48k exposed
        # via remotemic_native._C.upsample_16k_to_48k. Covers empty /
        # single-sample / multi-sample / carry-over state so steady-
        # state BLE notification cadence is parity-checked, not just
        # the first chunk. Per ADR-0014 §6 step 4:
        # "upsample_16k_to_48k_native_matches_python_byte_exact".
        add_test(
            NAME remotemic_upsample_parity
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/tests/bind"
                -p "test_upsample_16k_to_48k_parity.py"
                -v
        )
        set_tests_properties(remotemic_upsample_parity PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 4 / ADR-0014 §6 / step 4: audio route parity (G3).
        # Python FakePlaybackSink (apps/windows/rc003/tests/fakes/audio_route_fakes.py)
        # vs C++ FakeAudioRoute (remotemic_native._C.FakeAudioRoute).
        # Drives identical scripts through both recording doubles and
        # asserts sample-by-sample equality + lifecycle counter parity
        # + peak + RMS parity. Per ADR-0014 §6 step 4:
        # "sample-count / peak / RMS / drop-count / drain-order 与
        # Python baseline 对齐". This is build-time parity proof only;
        # real-device G6 validation (Typeless + RC003) is step 5/6.
        # Env-leak safety: REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=shadow
        # is set inside setUpClass with tearDown restore (5ce9bd5
        # corrective pattern).
        add_test(
            NAME remotemic_audio_route_parity
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_audio_route_native_parity.py"
                -v
        )
        set_tests_properties(remotemic_audio_route_parity PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 4 / ADR-0014 §6 / step 5: native switch + production
        # routing (G5). The ``audio_route_native.make_audio_route``
        # factory must route the production call site
        # (``RC003App._open_playback_for_new_session`` in app.py)
        # through ``_NativeAudioRoute`` when
        # ``REMOTEMIC_NATIVE_CHOICE_AUDIO_ROUTE=native`` is set,
        # and through ``EndpointPlaybackSink`` otherwise. ``shadow``
        # is forbidden for this module per plan §3 rule 5 (real
        # WASAPI device handle). Mirrors the Phase 3 step 5
        # native switch + production routing gates.
        add_test(
            NAME remotemic_phase4_native_switch
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase4_audio_route_native_switch.py"
                -v
        )
        set_tests_properties(remotemic_phase4_native_switch PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 4 / ADR-0014 §6 / step 5: production routing
        # closeout. Source-level proof that ``app.py`` references
        # ``make_audio_route`` and NOT the python
        # ``audio_playback.EndpointPlaybackSink`` directly. Same
        # regression-proof shape as ``test_phase3_production_routing.py``
        # for the voice / edge-debouncer / atvv-session trio.
        add_test(
            NAME remotemic_phase4_production_routing
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase4_audio_route_production_routing.py"
                -v
        )
        set_tests_properties(remotemic_phase4_production_routing PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 5 / ADR-0015 §6 / step 3: native switch for the input
        # source + host action sink (G5). The
        # ``input_source_native.make_input_source`` /
        # ``host_action_sink_native.make_host_action_sink`` factories
        # must route through the native shim when the corresponding
        # ``REMOTEMIC_NATIVE_CHOICE_*=native`` env var is set, and
        # through the python shim otherwise. ``shadow`` is forbidden
        # for both modules per plan §3 rule 5 (Raw Input handle +
        # SendInput dispatch are both side-effecting). Mirrors the
        # Phase 4 step 5 native switch gate.
        add_test(
            NAME remotemic_phase5_input_native_switch
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase5_input_native_switch.py"
                -v
        )
        set_tests_properties(remotemic_phase5_input_native_switch PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 5 / ADR-0015 §6 / step 3: production routing closeout.
        # Source-level proof that ``app.py`` references
        # ``make_input_source`` / ``make_host_action_sink`` and
        # constructs them in ``App.__init__`` so the env-var switch
        # takes effect at startup. Same regression-proof shape as
        # ``test_phase3_production_routing.py`` for the voice /
        # edge-debouncer / atvv-session trio and as
        # ``test_phase4_audio_route_production_routing.py`` for the
        # audio route.
        add_test(
            NAME remotemic_phase5_input_production_routing
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase5_input_production_routing.py"
                -v
        )
        set_tests_properties(remotemic_phase5_input_production_routing PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        add_test(
            NAME remotemic_phase6_ble_native_switch
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase6_ble_native_switch.py"
                -v
        )
        set_tests_properties(remotemic_phase6_ble_native_switch PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
            LABELS "phase6;ble;switch"
        )

        add_test(
            NAME remotemic_phase8_native_default
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase8_native_default.py"
                -v
        )
        set_tests_properties(remotemic_phase8_native_default PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
            LABELS "phase8;routing;rollback"
        )

        add_test(
            NAME remotemic_phase9_ui_native_state
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase9_ui_native_state.py"
                -v
        )
        set_tests_properties(remotemic_phase9_ui_native_state PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
            LABELS "phase9;ui;routing"
        )

        # Phase 3 / ADR-0013 step 4: runtime shadow parity tests
        # (G5). Each runs identical scripts through both the python
        # baseline and the C++ binding (via the bridge wrappers) and
        # asserts byte-exact equality. Env-leak safety: the env
        # override lives inside setUpClass with proper teardown
        # restore, matching the corrective fix pattern from
        # commit 5ce9bd5.
        # Helper script that bootstraps PYTHONPATH with both the build dir
        # (containing the compiled remotemic_native package) and the
        # source dir (containing ovb_rc003) before invoking unittest.
        # CMake's ENVIRONMENT property treats ``;`` as a list
        # separator and silently drops subsequent entries; this
        # wrapper sidesteps that by setting sys.path inside Python.
        set(_REMOTEMIC_PARITY_HELPER
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_parity_test.py"
        )
        set(_REMOTEMIC_PARITY_ENV
            "REMOTEMIC_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
            "REMOTEMIC_SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/src"
        )

        add_test(
            NAME remotemic_voice_controller_native_parity
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_voice_controller_native_parity.py"
                -v
        )
        set_tests_properties(remotemic_voice_controller_native_parity PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        add_test(
            NAME remotemic_voice_edge_debouncer_native_parity
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_voice_edge_debouncer_native_parity.py"
                -v
        )
        set_tests_properties(remotemic_voice_edge_debouncer_native_parity PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        add_test(
            NAME remotemic_atvv_session_native_parity
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_atvv_session_native_parity.py"
                -v
        )
        set_tests_properties(remotemic_atvv_session_native_parity PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 3 / ADR-0013 step 5: native switch + fake-backend
        # shadow verification. The bridge wrappers must (a) return
        # fresh instances per call (single-session owner contract)
        # and (b) route through the ``_C``-imported native class
        # without side effects. The fake backend is a runtime
        # monkey-patch that mimics the C++ surface so the test
        # asserts the bridge actually invokes the imported
        # ``remotemic_native.VoiceController`` /
        # ``remotemic_native.AtvvSession`` symbol.
        add_test(
            NAME remotemic_phase3_native_switch
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase3_native_switch.py"
                -v
        )
        set_tests_properties(remotemic_phase3_native_switch PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )

        # Phase 3 / ADR-0013 §5 production-routing closeout: verify
        # the three production call sites (app.py:_voice,
        # app.py:_voice_edge_debouncer, ble_transport_winrt.py:_session)
        # route through the make_* factories instead of constructing
        # the python classes directly. Default choice stays "python"
        # (no env override -> python baseline, no parallel native
        # instance); setting the three REMOTEMIC_NATIVE_CHOICE_* keys
        # to "native" routes the real product path through the bridge
        # shim holding exactly one C++ impl. Unsetting the env vars
        # returns to python (no residue). Source-level assertions
        # defend against a future regression re-introducing a direct
        # class reference.
        add_test(
            NAME remotemic_phase3_production_routing
            COMMAND ${Python3_EXECUTABLE} ${_REMOTEMIC_PARITY_HELPER}
                -m unittest
                -s "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/tests"
                -t "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003"
                -p "test_phase3_production_routing.py"
                -v
        )
        set_tests_properties(remotemic_phase3_production_routing PROPERTIES
            ENVIRONMENT "${_REMOTEMIC_PARITY_ENV}"
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Python binding (opt-in). Pulls pybind11, links remotemic_bind (a thin
# glue static lib that contains only translation code, no business rules)
# into the remotemic_native Python extension module.
# ---------------------------------------------------------------------------
if(REMOTEMIC_BUILD_PYTHON)
    # Python3_EXECUTABLE / PYTHON_EXECUTABLE / Python_EXECUTABLE were
    # set + ``find_package(Python3 ...)`` already ran at the top of the
    # file (before any ``add_test`` registration); CMake's
    # ``add_test(NAME foo COMMAND ${Python3_EXECUTABLE} ...)`` needs
    # the variable resolved at configure time, not later. The pybind11
    # FetchContent block below reuses those cached values.

    if(REMOTEMIC_PYBIND11_FETCH)
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.12.0
            GIT_SHALLOW TRUE
        )
        set(PYBIND11_NOPYTHON OFF CACHE BOOL "" FORCE)
        set(PYBIND11_INSTALL OFF CACHE BOOL "" FORCE)
        # Force the ABI tag to match the project's CPython 3.11 venv even
        # if pybind11's internal find_package picked up a different Python
        # (e.g. mise-installed 3.14). cp311 is the CPython 3.11 stable ABI.
        set(PYTHON_TAG "311" CACHE STRING "CPython ABI tag (cp311 for Python 3.11)" FORCE)
        FetchContent_MakeAvailable(pybind11)
    else()
        find_package(pybind11 CONFIG REQUIRED)
    endif()

    # remotemic_bind is an INTERFACE placeholder while binding code still
    # fits in one translation unit. It becomes a STATIC library when the
    # binding grows beyond bind_module.cpp.
    add_library(remotemic_bind INTERFACE)
    target_link_libraries(remotemic_bind INTERFACE remotemic_core pybind11::pybind11)

    # The C++ extension is the PRIVATE module remotemic_native._C per
    # ADR-0011. The public Python package wrapper (apps/windows/rc003/src/
    # remotemic_native/__init__.py) is what product code imports; _C is
    # the implementation detail. CMake compiles _C into
    # build/python/<Config>/remotemic_native/_C.cp311-win_amd64.pyd and
    # stages the wrapper __init__.py next to it so the build output is a
    # self-contained importable package.
    pybind11_add_module(remotemic_native_c MODULE src/bind/bind_module.cpp)
    set_target_properties(remotemic_native_c PROPERTIES
        OUTPUT_NAME "_C"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/remotemic_native"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/remotemic_native"
    )
    target_link_libraries(remotemic_native_c PRIVATE remotemic_bind remotemic_atvv remotemic_adpcm remotemic_voice remotemic_session remotemic_audio remotemic_input remotemic_ble remotemic_app remotemic_ui pybind11::pybind11)

    # Copy the public Python wrapper next to the compiled _C.pyd so the
    # build directory is a drop-in importable package.
    add_custom_command(TARGET remotemic_native_c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/src/remotemic_native/__init__.py"
            "$<TARGET_FILE_DIR:remotemic_native_c>/__init__.py"
        COMMENT "Staging remotemic_native/__init__.py next to _C.pyd"
    )
endif()
