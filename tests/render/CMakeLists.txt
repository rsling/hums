# The render core (render/snapshot/, TempoMap, Kernel, NoteTracker, Renderer)
# is std only, and so are its tests: they link the core and nothing of JUCE, so an
# edit there never pays a JUCE link (CLAUDE.md -> Toolchain).
# juce_recommended_warning_flags is compiler flags only.
hums_add_test_executable(render_core_tests
    KernelTest.cpp
    NoteTrackerTest.cpp
    RendererTest.cpp
    TempoMapTest.cpp)
target_link_libraries(render_core_tests PRIVATE
    hums_render_core
    juce::juce_recommended_warning_flags)
# One of them counts allocations: the render core takes nothing from the heap.
hums_count_allocations(render_core_tests)
target_include_directories(render_core_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
hums_discover_tests(render_core_tests "render_core.")

# The rest of render/ (SnapshotBuilder) reads the document, so its tests link
# the model layer and JUCE; never io/, the engine or the app.
hums_add_test_executable(render_tests
    SnapshotBuilderTest.cpp)
target_link_libraries(render_tests PRIVATE
    hums_render
    juce::juce_recommended_warning_flags)
hums_discover_tests(render_tests "render.")

# Guardrail 4, the render golden files (docs/GUARDRAILS.md -> 4. Render golden
# files): each fixture project in golden/ is loaded through the real load
# path, so this executable links io/ beside render/. Every case is also held
# to the render checks of tests/guardrails/.
hums_add_test_executable(render_golden_tests
    GoldenTest.cpp)
target_link_libraries(render_golden_tests PRIVATE
    hums_io
    hums_render
    juce::juce_recommended_warning_flags)
hums_add_render_checks(render_golden_tests)
target_include_directories(render_golden_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
# The tests read the pairs from the source tree, and regenerate_golden writes
# the expected files there.
target_compile_definitions(render_golden_tests PRIVATE
    HUMS_RENDER_GOLDEN_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/golden")
hums_discover_tests(render_golden_tests "render.")

# Writes the event lines of every expected file anew from what the renderer
# gives. Only ever run on purpose, and the diff is read before it is
# committed: a silently regenerated golden file turns a change of behaviour
# into a green build. CTest hands no arguments to a test, hence the variable.
add_custom_target(regenerate_golden
    COMMAND ${CMAKE_COMMAND} -E env HUMS_REGENERATE_GOLDEN=1 $<TARGET_FILE:render_golden_tests>
    DEPENDS render_golden_tests
    USES_TERMINAL
    VERBATIM)
