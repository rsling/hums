// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name where a constructor initialises a member that is a
// juce::Identifier. `label` is a text: "index" there is a value, not a name.
RowKey::RowKey()
    : rowProperty ("scene"), label ("index")
{
}
