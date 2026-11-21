// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: "activeTab" stands where the function takes an Identifier.
// "name" stands where it takes a value; that it is also a schema name does
// not make it a hit.
void Settings::setActiveTab (const juce::String& tabName)
{
    writeText ("activeTab", tabName.isEmpty() ? "name" : tabName);
}
