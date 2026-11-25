# The render core (render/snapshot/, TempoMap, Kernel, NoteTracker) is std
# only, and so are its tests: they link the core and nothing of JUCE, so an
# edit there never pays a JUCE link (CLAUDE.md -> Toolchain).
# juce_recommended_warning_flags is compiler flags only.
hums_add_test_executable(render_core_tests
    NoteTrackerTest.cpp
    TempoMapTest.cpp)
target_link_libraries(render_core_tests PRIVATE
    hums_render_core
    juce::juce_recommended_warning_flags)
hums_discover_tests(render_core_tests "render_core.")

# The rest of render/ (SnapshotBuilder) reads the document, so its tests link
# the model layer and JUCE; never io/, the engine or the app.
hums_add_test_executable(render_tests
    SnapshotBuilderTest.cpp)
target_link_libraries(render_tests PRIVATE
    hums_render
    juce::juce_recommended_warning_flags)
hums_discover_tests(render_tests "render.")
