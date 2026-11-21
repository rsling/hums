// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The literal may sit anywhere inside the call's brackets, on any line.
void setVelocity (juce::ValueTree& note, int velocity, juce::UndoManager& undoManager)
{
    note.setProperty (
        "velocity",
        velocity,
        &undoManager);
}
