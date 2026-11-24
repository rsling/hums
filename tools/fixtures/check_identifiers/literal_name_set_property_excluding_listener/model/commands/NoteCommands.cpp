// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is no schema name, and in model/ that is the violation [D15], also
// as the second argument of setPropertyExcludingListener.
void setSwing (juce::ValueTree& note, juce::ValueTree::Listener* listener)
{
    note.setPropertyExcludingListener (listener, "swing", 1, nullptr);
}
