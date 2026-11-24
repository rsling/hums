// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name in the list that a function returns as a list of structs
// of ours.
struct NodeInARow
{
    juce::ValueTree node;
    juce::Identifier rowProperty;
    int row = 0;
};

std::vector<NodeInARow> rowsOf (const juce::ValueTree& slot, const juce::ValueTree& scene)
{
    return { { slot, ids::scene, 0 }, { scene, "index", 0 } };
}
