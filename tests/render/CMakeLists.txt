# The render core (render/snapshot/, TempoMap, Kernel, NoteTracker) is std
# only, and so are its tests: they link the core and nothing of JUCE, so an
# edit there never pays a JUCE link (CLAUDE.md -> Toolchain).
# juce_recommended_warning_flags is compiler flags only.
hums_add_test_executable(render_core_tests
    TempoMapTest.cpp)
target_link_libraries(render_core_tests PRIVATE
    hums_render_core
    juce::juce_recommended_warning_flags)
hums_discover_tests(render_core_tests "render_core.")
