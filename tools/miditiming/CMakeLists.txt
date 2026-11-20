# tools/miditiming: loopback timing measurement (docs/ARCHITECTURE.md -> Timing).
# Built against the platform/ interfaces, so the engine's send path and any
# future backend are measured the same way. Plain Release build only.
add_executable(miditiming
    InputRecorder.cpp
    InputRecorder.h
    Main.cpp
    Sender.cpp
    Sender.h
    TimingReport.cpp
    TimingReport.h
    TrafficPattern.cpp
    TrafficPattern.h)

target_link_libraries(miditiming PRIVATE
    hums_platform_mac
    hums_juce
    juce::juce_recommended_warning_flags)
