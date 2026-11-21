// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a function of ours whose second
// parameter is a juce::Identifier; the check learns that from here.
class PropertyReader
{
public:
    static int getInt (const juce::ValueTree& node, const juce::Identifier& property, int valueWhenAbsent);
};
