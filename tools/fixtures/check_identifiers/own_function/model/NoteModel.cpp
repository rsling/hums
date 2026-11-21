// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name through a function of ours. juce::Identifier converts from
// the literal without a word, so this compiles.
int pitchOf (const juce::ValueTree& note)
{
    return PropertyReader::getInt (note, "pitch", 60);
}
