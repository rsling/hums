// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A reference to a juce::Identifier, bound to a literal: the literal becomes
// a temporary Identifier, so it is a name as in "juce::Identifier property = ...".
// The reference in line 9 is bound to a constant, and the text beside it is
// a text: no hit.
int pitchOf (const juce::ValueTree& note)
{
    const juce::Identifier& velocity = ids::velocity;
    const juce::String label = "velocity";
    const juce::Identifier& property = "pitch";
    return static_cast<int> (note.getProperty (property)) + static_cast<int> (note.getProperty (velocity)) + label.length();
}
