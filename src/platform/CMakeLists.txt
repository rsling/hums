# platform/: the std-only interfaces through which the engine reaches the OS
# (MidiScheduler, MonotonicClock, RealtimeThread). No JUCE, no OS headers.
hums_add_layer(hums_platform
    SOURCES
    LAYERS)
