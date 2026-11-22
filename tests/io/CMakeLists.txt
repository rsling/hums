# io/: one test file per IO format. Links the io layer and the model layer it
# stands on, never the engine or the app.
hums_add_test_executable(io_tests
    MigrationsTest.cpp
    ProjectFileTest.cpp)
target_link_libraries(io_tests PRIVATE
    hums_io
    juce::juce_recommended_warning_flags)
# The tests read their fixtures from the source tree.
target_compile_definitions(io_tests PRIVATE
    HUMS_IO_FIXTURES_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
hums_discover_tests(io_tests "io.")
