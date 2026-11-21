// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A lambda held in a variable is a function of ours too.
int lengthOf (const juce::ValueTree& note)
{
    const auto read = [&note] (const juce::Identifier& property) { return static_cast<int> (note[property]); };
    return read ("length");
}
