# platform/mac backends: the pure parts that need no MIDI hardware.
hums_add_test_executable(platform_tests
    DeviceIdentifierTest.cpp
    MidiBytesToUmpTest.cpp)
target_link_libraries(platform_tests PRIVATE
    hums_platform_mac
    juce::juce_recommended_warning_flags)
hums_discover_tests(platform_tests "platform.")
