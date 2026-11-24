// Fixture for tools/check_identifiers.py: this tree must fail the check.
// ValueTree::sendPropertyChangeMessage takes the name of a property.
void announcePitch (juce::ValueTree& note)
{
    note.sendPropertyChangeMessage ("pitch");
}
