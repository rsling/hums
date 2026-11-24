// Fixture for tools/check_identifiers.py: this tree must fail the check.
// juce::XmlDocument takes the tag its root must have as text.
std::unique_ptr<juce::XmlElement> parse (juce::XmlDocument& document)
{
    return document.getDocumentElementIfTagMatches ("Project");
}
