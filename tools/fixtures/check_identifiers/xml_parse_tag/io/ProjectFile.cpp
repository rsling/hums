// Fixture for tools/check_identifiers.py: this tree must fail the check.
// juce::parseXMLIfTagMatches takes the tag as its second argument. The first
// is the text to parse and no name.
std::unique_ptr<juce::XmlElement> emptyTimeline()
{
    return juce::parseXMLIfTagMatches ("<Timeline/>",
                                       "Timeline");
}
