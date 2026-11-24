// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name through a function of ours, in line 23. The two function
// types before it declare no function called "String": juce::String is their
// return type. So the texts in lines 17 and 18 are texts, not names handed
// to a function of ours, and no hit, although "pitch" is a schema name.
struct NoteDescriber
{
    std::function<juce::String (const juce::Identifier&)> describe;
};

using DescribeProperty = juce::String (const juce::Identifier&);

juce::String describeProperty (const juce::Identifier& property);

juce::String columnTitle()
{
    juce::String title ("pitch");
    return title + juce::String (" of the note");
}

juce::String pitchDescription()
{
    return describeProperty ("pitch");
}
