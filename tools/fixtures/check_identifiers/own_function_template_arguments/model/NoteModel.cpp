// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: "pitch" stands where the function takes an Identifier, and
// the template arguments before the bracket do not hide the call. "kind"
// stands where the function takes a value; that it is also a schema name
// does not make it a hit.
int pitchOf (const juce::ValueTree& note)
{
    return PropertyReader::readAs<int> (note, "pitch", 60);
}

juce::String kindOf (const juce::ValueTree& event)
{
    return PropertyReader::readAs<juce::String> (event, model::ids::kind, "kind");
}
