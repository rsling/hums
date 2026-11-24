// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name in the list in braces that a function returns as a struct
// of ours.
struct NodeInARow
{
    juce::ValueTree node;
    juce::Identifier rowProperty;
    int row = 0;
};

NodeInARow rowOfSlot (const juce::ValueTree& slot)
{
    return { slot, "scene", 0 };
}
