# engine/: Transport, Clock, Scheduler, MidiRouter, FIFOs, snapshot handoff.
# Never model/, never render/ outside the core, never platform/mac/.
hums_add_layer(hums_engine
    SOURCES
    LAYERS hums_render_core hums_platform
    JUCE)
