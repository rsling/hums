// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. It declares a struct of ours whose second
// member is a juce::Identifier; the check learns that from here.
struct NodeInARow
{
    juce::ValueTree node;
    juce::Identifier rowProperty;
    int row = 0;
};
