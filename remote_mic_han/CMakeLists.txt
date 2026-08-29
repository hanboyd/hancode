cmake_minimum_required(VERSION 3.25)

project(RemoteMicWindows VERSION 0.2.0 LANGUAGES CXX)

option(REMOTEMIC_BUILD_TESTS "Build hardware-free C++ tests" ON)
option(REMOTEMIC_BUILD_PYTHON "Build the CPython binding extension" OFF)
option(REMOTEMIC_PYBIND11_FETCH "Fetch pybind11 via FetchContent (else find_package)" ON)

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
# Win32 platform layer: filesystem, env, logger, Application::diagnose.
# May include <windows.h> and other OS headers. Consumes remotemic_core.
# ---------------------------------------------------------------------------
add_library(remotemic_platform_win32 STATIC
    src/app/application.cpp
    src/logging/file_logger.cpp
    src/platform/runtime_paths.cpp
)

target_link_libraries(remotemic_platform_win32 PUBLIC remotemic_core)

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
    endif()
endif()

# ---------------------------------------------------------------------------
# Python binding (opt-in). Pulls pybind11, links remotemic_bind (a thin
# glue static lib that contains only translation code, no business rules)
# into the remotemic_native Python extension module.
# ---------------------------------------------------------------------------
if(REMOTEMIC_BUILD_PYTHON)
    # Find the project's pinned CPython 3.11 BEFORE pulling pybind11 so
    # Python3_VERSION reflects the venv interpreter. Pybind11 caches its
    # Python detection on first find_package and a later override does not
    # propagate to the .pyd ABI tag.
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
    target_link_libraries(remotemic_native_c PRIVATE remotemic_bind remotemic_atvv pybind11::pybind11)

    # Copy the public Python wrapper next to the compiled _C.pyd so the
    # build directory is a drop-in importable package.
    add_custom_command(TARGET remotemic_native_c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/windows/rc003/src/remotemic_native/__init__.py"
            "$<TARGET_FILE_DIR:remotemic_native_c>/__init__.py"
        COMMENT "Staging remotemic_native/__init__.py next to _C.pyd"
    )
endif()