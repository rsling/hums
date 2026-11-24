// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A node's type compared with a text names the type, as hasType does. The
// second comparison is of two texts, where a schema name is no hit.
bool isTrack (const juce::ValueTree& node, const juce::String& text)
{
    return node.getType() == "Track" && text != "Clip";
}
