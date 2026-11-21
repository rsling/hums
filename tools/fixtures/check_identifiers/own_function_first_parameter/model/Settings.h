// Fixture for tools/check_identifiers.py: part of a tree that must fail the
// check. This file is clean. Here the juce::Identifier is the first
// parameter, not the second, and the function is not one of PropertyReader's.
class Settings
{
public:
    void setActiveTab (const juce::String& tabName);

private:
    void writeText (const juce::Identifier& property, const juce::String& text);
};
