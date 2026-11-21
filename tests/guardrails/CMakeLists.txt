# Toolchain smoke tests: prove that every configuration builds real code, with
# and without JUCE, before any layer has sources, and that it runs wherever the
# gate runs every test (plain and ASan + UBSan). The TSan leg of the gate
# builds them too but runs only engine tests and the canary (tools/gate.sh).

# std only, no JUCE: the shape of every future render-core test.
hums_add_test_executable(guardrail_toolchain_std ToolchainStdTest.cpp)
hums_discover_tests(guardrail_toolchain_std "guardrail.toolchain_std.")

# The pinned JUCE, built in this configuration (instrumented or not) through
# hums_juce: the same library the layers link.
hums_add_test_executable(guardrail_toolchain_juce ToolchainJuceTest.cpp)
target_link_libraries(guardrail_toolchain_juce PRIVATE
    hums_juce
    juce::juce_recommended_warning_flags)
hums_discover_tests(guardrail_toolchain_juce "guardrail.toolchain_juce.")

# The negative tests for HUMS_JUCE_REPORT_REGEX (tests/CMakeLists.txt): one
# program with a failed jassert and one with a leaked object. Each exits 0 and
# is expected to fail, so it passes only while JUCE's printed report still
# fails a test. Debug only: jassert and the leak detector are compiled out of
# a Release build.
function(hums_add_juce_report_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
        hums_juce
        juce::juce_recommended_warning_flags)
    add_dependencies(hums_tests ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        FAIL_REGULAR_EXPRESSION "${HUMS_JUCE_REPORT_REGEX}"
        WILL_FAIL TRUE)
endfunction()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    hums_add_juce_report_test(guardrail_juce_report_assertion JuceReportAssertion.cpp)
    hums_add_juce_report_test(guardrail_juce_report_leak JuceReportLeak.cpp)
endif()

if(HUMS_SANITIZE)
    add_subdirectory(canaries)
endif()

# ---------------------------------------------------------------------------
# Guardrail scripts (docs/GUARDRAILS.md). Each script in tools/ is registered
# twice: once against the repository, where it must find nothing, and once
# against its fixture in tools/fixtures/, where it must report a violation.
#
# The scripts run with the Python that ships with the pinned Xcode
# (TOOLCHAIN.md), through xcrun like clang-format, so a MacPorts or Homebrew
# Python on the PATH plays no role.
find_program(HUMS_XCRUN xcrun REQUIRED)

# The script must exit 0.
function(hums_add_script_guardrail name script)
    add_test(NAME ${name}
        COMMAND "${HUMS_XCRUN}" python3 "${PROJECT_SOURCE_DIR}/tools/${script}" ${ARGN})
endfunction()

# The script must print its violation line. Judged by that line and not by the
# exit code (WILL_FAIL), because a script that crashes, or a missing Python,
# also exits non-zero and would pass for a detected violation.
function(hums_add_script_guardrail_fixture name script violation_regex)
    add_test(NAME ${name}
        COMMAND "${HUMS_XCRUN}" python3 "${PROJECT_SOURCE_DIR}/tools/${script}" ${ARGN})
    set_tests_properties(${name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${violation_regex}")
endfunction()

# JUCE pin: libs/JUCE is at the tag libs/JUCE_VERSION names. The fixture is a
# version file naming another tag.
hums_add_script_guardrail(guardrail_juce_pin check_juce_pin.py)
hums_add_script_guardrail_fixture(guardrail_juce_pin_fixture_fails check_juce_pin.py
    "check_juce_pin: VIOLATION: .*pins JUCE '9\\.0\\.5'"
    --version-file "${PROJECT_SOURCE_DIR}/tools/fixtures/check_juce_pin/JUCE_VERSION")
