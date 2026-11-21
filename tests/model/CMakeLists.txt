# model/: one test file per model type. Links the model layer only, never
# io/, render/ or the engine.
hums_add_test_executable(model_tests
    PropertyReaderTest.cpp)
target_link_libraries(model_tests PRIVATE
    hums_model
    juce::juce_recommended_warning_flags)
hums_discover_tests(model_tests "model.")
