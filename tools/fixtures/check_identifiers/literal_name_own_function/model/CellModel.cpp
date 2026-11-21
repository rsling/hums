// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is not a schema name, and that is the point: in model/ a property
// that has no constant in Ids.h is an invented one, also when it is only read.
int swingOf (const juce::ValueTree& cell)
{
    return PropertyReader::getRequiredInt (cell, "swing");
}
