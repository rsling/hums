// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The tag name of an XML element that is being made.
std::unique_ptr<juce::XmlElement> makeNoteElement()
{
    return std::make_unique<juce::XmlElement> ("Note");
}
