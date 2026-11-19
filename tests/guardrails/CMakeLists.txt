# Toolchain smoke tests: prove that every configuration builds and runs real
# code, with and without JUCE, before any layer has sources.

# std only, no JUCE: the shape of every future render-core test.
hums_add_test_executable(guardrail_toolchain_std ToolchainStdTest.cpp)
catch_discover_tests(guardrail_toolchain_std TEST_PREFIX "guardrail.toolchain_std.")

# The pinned JUCE, built and run in this configuration (instrumented or not).
hums_add_test_executable(guardrail_toolchain_juce ToolchainJuceTest.cpp)
target_link_libraries(guardrail_toolchain_juce PRIVATE
    juce::juce_core
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
target_compile_definitions(guardrail_toolchain_juce PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)
catch_discover_tests(guardrail_toolchain_juce TEST_PREFIX "guardrail.toolchain_juce.")
