# render/ is two layers.
#
# The render core (render/snapshot/, TempoMap, Kernel, NoteTracker, Renderer)
# is std only: no JUCE, no model/. The engine links it, and render-core tests link
# nothing else.
hums_add_layer(hums_render_core
    SOURCES
        Kernel.cpp
        Kernel.h
        NoteTracker.cpp
        NoteTracker.h
        Renderer.cpp
        Renderer.h
        TempoMap.cpp
        TempoMap.h
        snapshot/ClipTemplate.h
        snapshot/Snapshot.h
        snapshot/TrackPlan.h
    LAYERS)

# The rest of render/ (SnapshotBuilder): document -> Snapshot, message thread.
hums_add_layer(hums_render
    SOURCES
        SnapshotBuilder.cpp
        SnapshotBuilder.h
    LAYERS hums_render_core hums_model
    JUCE)
