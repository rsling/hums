# render/ is two layers.
#
# The render core (render/snapshot/, TempoMap, Kernel, NoteTracker) is std
# only: no JUCE, no model/. The engine links it, and render-core tests link
# nothing else.
hums_add_layer(hums_render_core
    SOURCES
    LAYERS)

# The rest of render/ (SnapshotBuilder): document -> Snapshot, message thread.
hums_add_layer(hums_render
    SOURCES
    LAYERS hums_render_core hums_model
    JUCE)
