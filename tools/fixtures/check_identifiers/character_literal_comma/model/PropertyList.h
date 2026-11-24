// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a function of ours whose second
// parameter is a juce::Identifier; the check learns that from here.
class PropertyList
{
public:
    static juce::String joined (char separator, const juce::Identifier& property, const juce::ValueTree& nodes);
};
