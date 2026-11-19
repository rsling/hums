# Toolchain smoke tests: prove that every configuration builds and runs real
# code, with and without JUCE, before any layer has sources.

# std only, no JUCE: the shape of every future render-core test.
hums_add_test_executable(guardrail_toolchain_std ToolchainStdTest.cpp)
catch_discover_tests(guardrail_toolchain_std TEST_PREFIX "guardrail.toolchain_std.")

# The pinned JUCE, built and run in this configuration (instrumented or not),
# through hums_juce: the same library the layers link.
hums_add_test_executable(guardrail_toolchain_juce ToolchainJuceTest.cpp)
target_link_libraries(guardrail_toolchain_juce PRIVATE
    hums_juce
    juce::juce_recommended_warning_flags)
catch_discover_tests(guardrail_toolchain_juce TEST_PREFIX "guardrail.toolchain_juce.")

if(HUMS_SANITIZE)
    add_subdirectory(canaries)
endif()
