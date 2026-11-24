// Fixture for tools/check_identifiers.py: this tree must fail the check.
// juce::CachedValue::referTo takes the name as its second argument.
void watch (juce::CachedValue<int>& pitch, juce::ValueTree& note)
{
    pitch.referTo (note, "pitch", nullptr);
}
