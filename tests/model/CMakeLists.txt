# model/: one test file per model type. Links the model layer only, never
# io/, render/ or the engine.
hums_add_test_executable(model_tests
    AutomationClipRuleTest.cpp
    AutomationLaneCommandsTest.cpp
    AutomationLaneModelTest.cpp
    CanonicalDoubleTest.cpp
    CcValueModelTest.cpp
    CellModelTest.cpp
    ClipModelTest.cpp
    ClipPoolCommandsTest.cpp
    ClipPoolModelTest.cpp
    CloneMarkTest.cpp
    ColourTextTest.cpp
    CopyNameTest.cpp
    CommandFixture.h
    DeclaredIdentifiersTest.cpp
    DefaultValueUndoStepTest.cpp
    EditContextTest.cpp
    EventClipCommandsTest.cpp
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
    PlacementCommandsTest.cpp
    PlacementModelTest.cpp
    PlayerModelTest.cpp
    ProjectModelTest.cpp
    ProjectCommandsTest.cpp
    PropertyReaderTest.cpp
    QuantiseTest.cpp
    RefcountGateTest.cpp
    RoutingCommandsTest.cpp
    RoutingModelTest.cpp
    SceneModelTest.cpp
    SessionCommandsTest.cpp
    SessionModelTest.cpp
    SettingsTest.cpp
    SiblingOrderTest.cpp
    SlotCommandsTest.cpp
    SlotModelTest.cpp
    StaleEditContextTest.cpp
    StepClipCommandsTest.cpp
    TempoEventModelTest.cpp
    TimeSigEventModelTest.cpp
    TempoRangeTest.cpp
    TimelineCommandsTest.cpp
    TimelineModelTest.cpp
    TrackCommandsTest.cpp
    TrackModelTest.cpp
    UnusedEventFieldsTest.cpp
    TracksModelTest.cpp
    ValidatorTest.cpp)
target_link_libraries(model_tests PRIVATE
    hums_model
    juce::juce_recommended_warning_flags)
# DeclaredIdentifiersTest reads Ids.h itself, to hold the list to it.
target_compile_definitions(model_tests PRIVATE
    HUMS_IDS_HEADER="${PROJECT_SOURCE_DIR}/src/model/Ids.h")
hums_discover_tests(model_tests "model.")
