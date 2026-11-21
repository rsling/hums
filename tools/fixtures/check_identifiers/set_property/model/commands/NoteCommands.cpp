// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The name may stand on a line of its own, away from the call's bracket.
void setVelocity (juce::ValueTree& note, int velocity, juce::UndoManager& undoManager)
{
    note.setProperty (
        "velocity",
        velocity,
        &undoManager);
}
