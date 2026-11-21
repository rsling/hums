// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is not a schema name: an attribute Hums would never write is an
// invented name, like a property that has no constant in Ids.h.
int swingOf (const juce::XmlElement& xml)
{
    return xml.getIntAttribute ("swing", 0);
}
