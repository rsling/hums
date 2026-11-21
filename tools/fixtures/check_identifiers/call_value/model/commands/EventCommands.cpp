// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: "kind" stands where setProperty takes the name. "cc" stands
// where it takes the value; that it is also a schema name (CcValue.cc,
// Knob.cc) does not make it a hit, and no more does the default in
// getProperty.
void makeCcEvent (juce::ValueTree& event, juce::UndoManager& undoManager)
{
    if (event.getProperty (model::ids::kind, "cc").toString().isEmpty())
        return;

    event.setProperty ("kind", "cc", &undoManager);
}
