// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The comma in ',' is a character and separates no arguments: "pitch" is
// the second argument of a function of ours, which is a juce::Identifier.
juce::String pitchesOf (const juce::ValueTree& notes)
{
    return PropertyList::joined (',', "pitch", notes);
}
