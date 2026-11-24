// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A juce::CachedValue is constructed from a tree and the name of a property;
// the name is the second argument. The default value in the first one equals
// a schema name and is no hit.
void watch (juce::ValueTree& note)
{
    juce::CachedValue<juce::String> kind (note, ids::kind, nullptr, "cc");
    juce::CachedValue<int> pitch (note, "pitch", nullptr);
}
