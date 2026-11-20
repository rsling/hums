cmake_minimum_required(VERSION 3.31)

# Must be set before project(): CMake reads it when it first configures the compilers.
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum macOS version (pinned in TOOLCHAIN.md)")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture")

project(Hums VERSION 0.0.1 LANGUAGES C CXX OBJC OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# ccache: the compiler launcher for every language (TOOLCHAIN.md).
# Required, so a missing ccache shows up at configure time instead of as slow builds.
find_program(HUMS_CCACHE ccache REQUIRED)
foreach(lang C CXX OBJC OBJCXX)
    set(CMAKE_${lang}_COMPILER_LAUNCHER "${HUMS_CCACHE}")
endforeach()

# ---------------------------------------------------------------------------
# Sanitizer configurations (docs/GUARDRAILS.md -> Sanitizer builds).
# The flags are set here, before JUCE and Catch2 are added, so that everything
# linked into a test binary is instrumented.
set(HUMS_SANITIZE "" CACHE STRING "Sanitizer configuration: empty (plain), address (ASan + UBSan) or thread (TSan)")
set_property(CACHE HUMS_SANITIZE PROPERTY STRINGS "" address thread)

if(HUMS_SANITIZE STREQUAL "address")
    set(HUMS_SANITIZER_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
elseif(HUMS_SANITIZE STREQUAL "thread")
    set(HUMS_SANITIZER_FLAGS -fsanitize=thread -fno-omit-frame-pointer)
elseif(NOT HUMS_SANITIZE STREQUAL "")
    message(FATAL_ERROR "HUMS_SANITIZE must be empty, 'address' or 'thread', not '${HUMS_SANITIZE}'")
endif()

if(HUMS_SANITIZE)
    # The instrumented configurations are single-config Ninja Debug builds in
    # their own directories (build-asan, build-tsan).
    if(CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "HUMS_SANITIZE needs a single-config generator (Ninja), not ${CMAKE_GENERATOR}")
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "HUMS_SANITIZE needs CMAKE_BUILD_TYPE=Debug")
    endif()
    add_compile_options(${HUMS_SANITIZER_FLAGS})
    add_link_options(${HUMS_SANITIZER_FLAGS})
    list(JOIN HUMS_SANITIZER_FLAGS " " hums_flags_text)
    message(STATUS "Hums: sanitizer configuration '${HUMS_SANITIZE}': ${hums_flags_text}")
endif()

# ---------------------------------------------------------------------------
# Third-party code: JUCE (pinned submodule) and Catch2 (pinned FetchContent tag).
add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)

# hums_juce: the pinned JUCE modules, compiled once. JUCE modules are INTERFACE
# libraries that add their sources to whatever links them, and must be linked
# PRIVATE (libs/JUCE/docs/CMake API.md -> juce_add_module); linking them into
# several layer libraries would build one copy per layer into each binary.
# So only hums_juce links them, and it re-exports their include paths and
# definitions to the layers and tests that link it. The linker pulls in only the
# modules a binary uses. Modules are added here as layers need them; never a
# juce_gui_* module (tests must not link GUI code).
add_library(hums_juce STATIC)
target_link_libraries(hums_juce
    PRIVATE
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_audio_basics
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags)
target_compile_definitions(hums_juce PUBLIC
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)
target_include_directories(hums_juce INTERFACE
    $<TARGET_PROPERTY:hums_juce,INCLUDE_DIRECTORIES>)
target_compile_definitions(hums_juce INTERFACE
    $<TARGET_PROPERTY:hums_juce,COMPILE_DEFINITIONS>)

include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

# ---------------------------------------------------------------------------
# The layer libraries.
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Tests. hums_tests builds every test executable and nothing of the app.
enable_testing()
add_custom_target(hums_tests)

add_subdirectory(tests)

# ---------------------------------------------------------------------------
# Tools. Timing is never measured on an instrumented build
# (docs/GUARDRAILS.md -> Sanitizer builds), so miditiming isn't built there.
if(NOT HUMS_SANITIZE)
    add_subdirectory(tools/miditiming)
endif()
