# platform/mac/: the only backends, and the only place OS headers and .mm files
# appear. Constructed by app/ (and by tools/miditiming), never named by the engine.
hums_add_layer(hums_platform_mac
    SOURCES
        CoreMidiScheduler.cpp
        CoreMidiScheduler.h
        DeviceIdentifier.cpp
        DeviceIdentifier.h
        MachMonotonicClock.cpp
        MachMonotonicClock.h
        MachRealtimeThread.cpp
        MachRealtimeThread.h
        MachTime.cpp
        MachTime.h
        MidiBytesToUmp.cpp
        MidiBytesToUmp.h
    LAYERS hums_platform)

target_link_libraries(hums_platform_mac PUBLIC
    "-framework CoreMIDI"
    "-framework CoreFoundation")
