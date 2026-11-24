// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name through a member of a struct of ours. The struct is filled
// from a list in braces, and juce::Identifier converts from the literal
// without a word. The last member is a text: "cc" there is a value, not a name.
struct PropertyRule
{
    juce::Identifier nodeType;
    juce::Identifier property;
    juce::String defaultText;
};

const PropertyRule kindRule { ids::Event, "kind", "cc" };
