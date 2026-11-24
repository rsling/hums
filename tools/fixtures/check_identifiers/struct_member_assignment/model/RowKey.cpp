// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name assigned to a member that is a juce::Identifier. `label` is
// a text: "index" there is a value, not a name.
struct RowKey
{
    juce::Identifier rowProperty;
    juce::String label;
};

void nameIt (RowKey& key)
{
    key.label = "index";
    key.rowProperty = "scene";
}
