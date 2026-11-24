// Fixture for tools/check_identifiers.py: this tree must fail the check.
// As struct_member_push_back, with emplace_back, which takes what would
// stand in the braces.
struct NodeInARow
{
    juce::ValueTree node;
    juce::Identifier rowProperty;
    int row = 0;
};

std::vector<NodeInARow> nodesInRows (const juce::ValueTree& slot)
{
    std::vector<NodeInARow> found;
    found.emplace_back (slot, "scene", 0);
    return found;
}
