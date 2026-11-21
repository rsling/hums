// Fixture for tools/check_identifiers.py: this tree must fail the check.
// juce::XmlElement takes a tag name as text. In io/ the XML is the project
// file, so the name is a schema name all the same.
bool isProject (const juce::XmlElement& xml)
{
    return xml.hasTagName ("Project");
}
