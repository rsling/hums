// Fixture for tools/check_identifiers.py: this tree must fail the check.
bool isNote (const juce::ValueTree& node)
{
    return node.hasType ("Note");
}
