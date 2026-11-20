# platform/: the std-only interfaces through which the engine reaches the OS
# (MidiScheduler, MonotonicClock, RealtimeThread). No JUCE, no OS headers.
# Header-only, so the layer is an INTERFACE library (see hums_add_layer).
hums_add_layer(hums_platform
    SOURCES
        MidiScheduler.h
        MonotonicClock.h
        RealtimeThread.h
    LAYERS)
