# tools/miditiming: loopback timing measurement (docs/ARCHITECTURE.md -> Timing).
# Built against the platform/ interfaces, so the engine's send path and any
# future backend are measured the same way. Plain Release build only.
add_executable(miditiming
    InputRecorder.cpp
    InputRecorder.h
    ListenReport.cpp
    ListenReport.h
    Main.cpp
    ReferenceFile.cpp
    ReferenceFile.h
    Sender.cpp
    Sender.h
    Statistics.cpp
    Statistics.h
    TimingReport.cpp
    TimingReport.h
    TrafficPattern.cpp
    TrafficPattern.h)

target_link_libraries(miditiming PRIVATE
    hums_platform_mac
    hums_juce
    juce::juce_recommended_warning_flags)
