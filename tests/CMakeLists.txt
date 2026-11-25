# Each test executable is registered with hums_tests so that the fast loop
# builds every test and nothing of the app.
function(hums_add_test_executable target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    add_dependencies(hums_tests ${target})
endfunction()

# What JUCE prints when a jassert fails or its leak detector finds a leaked
# object. Outside a debugger JUCE only prints these and the process still
# exits 0, so without this pattern CTest would call such a test passed
# (docs/GUARDRAILS.md -> Sanitizer builds). The exact texts are in
# juce_Logger.cpp (logAssertion) and juce_LeakedObjectDetector.h.
set(HUMS_JUCE_REPORT_REGEX "JUCE Assertion failure|Leaked objects detected")

# Registers the Catch2 test cases of a test executable with CTest. Every test
# executable goes through here, never through a bare catch_discover_tests, so
# that every test fails on a JUCE report.
function(hums_discover_tests target prefix)
    catch_discover_tests(${target}
        TEST_PREFIX "${prefix}"
        PROPERTIES FAIL_REGULAR_EXPRESSION "${HUMS_JUCE_REPORT_REGEX}")
endfunction()

# Lets a test executable count what it takes from the heap
# (tests/guardrails/AllocationCounter.h), for the tests that show the render
# core to allocate nothing. Counting replaces the program's operator new, so
# it is done in the plain configuration only: in an instrumented one the
# sanitizer's own operator new stays, and AllocationCounter counts nothing.
function(hums_count_allocations target)
    target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/guardrails/AllocationCounter.cpp"
        "${PROJECT_SOURCE_DIR}/tests/guardrails/AllocationCounter.h")
    if(NOT HUMS_SANITIZE)
        target_compile_definitions(${target} PRIVATE HUMS_COUNT_ALLOCATIONS=1)
    endif()
endfunction()

# The render checks (tests/guardrails/RenderChecks.h) and the run they play a
# Snapshot with, for the two executables that render documents: the golden
# tests and the property tests. Std and the render core only; they count
# allocations.
function(hums_add_render_checks target)
    target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/guardrails/RenderChecks.cpp"
        "${PROJECT_SOURCE_DIR}/tests/guardrails/RenderChecks.h"
        "${PROJECT_SOURCE_DIR}/tests/guardrails/RenderRun.cpp"
        "${PROJECT_SOURCE_DIR}/tests/guardrails/RenderRun.h")
    hums_count_allocations(${target})
endfunction()

add_subdirectory(guardrails)
add_subdirectory(io)
add_subdirectory(model)
add_subdirectory(platform)
add_subdirectory(render)
