// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a function template of ours whose
// second parameter is a juce::Identifier; the check learns that from here.
class PropertyReader
{
public:
    template <typename Value>
    static Value readAs (const juce::ValueTree& node, const juce::Identifier& property, Value valueWhenAbsent);
};
