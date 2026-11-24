// Fixture for tools/check_identifiers.py: this tree must fail the check.
// ValueTree::setPropertyExcludingListener takes the name as its second
// argument. The value in the first call equals a schema name and is no hit.
void setKind (juce::ValueTree& event, juce::ValueTree::Listener* listener, juce::UndoManager& undoManager)
{
    event.setPropertyExcludingListener (listener, ids::kind, "cc", &undoManager);
    event.setPropertyExcludingListener (listener, "kind", cc, &undoManager);
}
