// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A property is read through model::ids, never by a spelled-out name.
int pitchOf (const juce::ValueTree& note)
{
    return note.getProperty ("pitch");
}
