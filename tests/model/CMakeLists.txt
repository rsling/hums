# model/: one test file per model type. Links the model layer only, never
# io/, render/ or the engine.
hums_add_test_executable(model_tests
    AutomationLaneModelTest.cpp
    CcValueModelTest.cpp
    CellModelTest.cpp
    ClipModelTest.cpp
    ClipPoolModelTest.cpp
    EventModelTest.cpp
    GridModelTest.cpp
    KnobModelTest.cpp
    LaneModelTest.cpp
    MarkerModelTest.cpp
    NoteModelTest.cpp
    PadMapModelTest.cpp
    PadModelTest.cpp
    PerformerModelTest.cpp
    PlacementModelTest.cpp
    PlayerModelTest.cpp
    ProjectModelTest.cpp
    PropertyReaderTest.cpp
    RoutingModelTest.cpp
    SceneModelTest.cpp
    SessionModelTest.cpp
    SettingsTest.cpp
    SlotModelTest.cpp
    TempoEventModelTest.cpp
    TimeSigEventModelTest.cpp
    TimelineModelTest.cpp
    TrackModelTest.cpp
    TracksModelTest.cpp)
target_link_libraries(model_tests PRIVATE
    hums_model
    juce::juce_recommended_warning_flags)
hums_discover_tests(model_tests "model.")
