// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The same comparison with the text first, and with !=.
bool isNoClip (const juce::ValueTree& node)
{
    return "Clip" != node.getType();
}
