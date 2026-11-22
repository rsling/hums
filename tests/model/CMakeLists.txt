# model/: one test file per model type. Links the model layer only, never
# io/, render/ or the engine.
hums_add_test_executable(model_tests
    AutomationLaneModelTest.cpp
    CanonicalDoubleTest.cpp
    CcValueModelTest.cpp
    CellModelTest.cpp
    ClipModelTest.cpp
    ClipPoolModelTest.cpp
    CommandFixture.h
    DeclaredIdentifiersTest.cpp
    EventModelTest.cpp
    GridModelTest.cpp
    KnobModelTest.cpp
    LaneModelTest.cpp
    MarkerModelTest.cpp
    NodeLookupTest.cpp
    NoteModelTest.cpp
    PadMapCommandsTest.cpp
    PadMapModelTest.cpp
    PadModelTest.cpp
    PerformerModelTest.cpp
    PlacementModelTest.cpp
    PlayerModelTest.cpp
    ProjectModelTest.cpp
    PropertyReaderTest.cpp
    RoutingCommandsTest.cpp
    RoutingModelTest.cpp
    SceneModelTest.cpp
    SessionCommandsTest.cpp
    SessionModelTest.cpp
    SettingsTest.cpp
    SlotModelTest.cpp
    TempoEventModelTest.cpp
    TimeSigEventModelTest.cpp
    TimelineCommandsTest.cpp
    TimelineModelTest.cpp
    TrackCommandsTest.cpp
    TrackModelTest.cpp
    TracksModelTest.cpp
    ValidatorTest.cpp)
target_link_libraries(model_tests PRIVATE
    hums_model
    juce::juce_recommended_warning_flags)
# DeclaredIdentifiersTest reads Ids.h itself, to hold the list to it.
target_compile_definitions(model_tests PRIVATE
    HUMS_IDS_HEADER="${PROJECT_SOURCE_DIR}/src/model/Ids.h")
hums_discover_tests(model_tests "model.")
