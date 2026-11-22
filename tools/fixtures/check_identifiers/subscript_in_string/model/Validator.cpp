// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: cell["gate"] is a subscript with a name in it. The brackets
// in pathOf are text inside two string literals, and what stands between
// them is no name.
juce::String pathOf (const juce::ValueTree& node, int position)
{
    return node.getType().toString() + "[" + juce::String (position) + "]";
}

double gateOf (const juce::ValueTree& cell)
{
    return cell["gate"];
}
