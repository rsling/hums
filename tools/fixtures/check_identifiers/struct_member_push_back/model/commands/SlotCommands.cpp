// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name in one more element of a list of structs of ours. The check
// knows `found` as such a list by its declaration.
std::vector<NodeInARow> nodesInRows (const juce::ValueTree& slot, const juce::ValueTree& scene)
{
    std::vector<NodeInARow> found;
    found.push_back ({ slot, "scene", 0 });
    found.push_back ({ scene, ids::index, 0 });
    return found;
}
