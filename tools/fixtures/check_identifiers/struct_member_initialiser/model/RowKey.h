// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a class of ours with a member that
// is a juce::Identifier; the check learns that from here.
class RowKey
{
public:
    RowKey();

private:
    juce::Identifier rowProperty;
    juce::String label;
};
