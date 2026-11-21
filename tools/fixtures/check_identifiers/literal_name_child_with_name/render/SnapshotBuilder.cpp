// Fixture for tools/check_identifiers.py: this tree must fail the check.
// An invented child type, read in render/.
juce::ValueTree extrasOf (const juce::ValueTree& clip)
{
    return clip.getChildWithName ("Extras");
}
