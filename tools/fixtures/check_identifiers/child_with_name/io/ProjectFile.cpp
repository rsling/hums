// Fixture for tools/check_identifiers.py: this tree must fail the check.
juce::ValueTree clipPoolOf (const juce::ValueTree& project)
{
    return project.getChildWithName ("ClipPool");
}
