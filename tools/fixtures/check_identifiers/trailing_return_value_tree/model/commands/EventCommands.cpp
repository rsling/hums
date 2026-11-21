// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: the brace after "-> juce::ValueTree" opens the body of the
// lambda, not the brackets of a ValueTree construction, so "cc", the value of
// kind, is no hit here either. "tick" in the place of the name is the hit.
static const auto makeCcEvent = [] (juce::int64 tick, juce::UndoManager& undoManager) -> juce::ValueTree {
    juce::ValueTree event (model::ids::Event);
    event.setProperty (model::ids::kind, "cc", &undoManager);
    event.setProperty ("tick", tick, &undoManager);
    return event;
};
