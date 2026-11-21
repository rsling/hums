// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is not a schema name: an invented property, given to a ValueTree
// as it is constructed.
juce::ValueTree makeNote()
{
    return juce::ValueTree { model::ids::Note, { { "swing", 1 } } };
}
