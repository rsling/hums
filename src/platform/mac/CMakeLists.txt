# platform/mac/: the only backends, and the only place OS headers and .mm files
# appear. Constructed by app/, never named by the engine.
hums_add_layer(hums_platform_mac
    SOURCES
    LAYERS hums_platform
    JUCE)
