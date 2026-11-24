// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A juce::ValueTreePropertyWithDefault is constructed like a juce::CachedValue:
// a tree, then the name of a property.
void watch (juce::ValueTree& cell)
{
    juce::ValueTreePropertyWithDefault gate { cell, "gate", nullptr };
}
