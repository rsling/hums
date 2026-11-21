// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is not a schema name, and that is the point: in model/ a property
// that has no constant in Ids.h is an invented one.
void setSwing (juce::ValueTree& note, int swing, juce::UndoManager& undoManager)
{
    note.setProperty ("swing", swing, &undoManager);
}
