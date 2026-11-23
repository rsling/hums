// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a class of ours whose constructor
// takes a juce::Identifier; the check learns that from here.
class PropertyName
{
public:
    explicit PropertyName (const juce::Identifier& name);
};
