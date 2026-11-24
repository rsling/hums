// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name in a list of structs of ours, each filled from a list in
// braces. "cc" in the first line is the default text, a value and not a name.
struct PropertyRule
{
    juce::Identifier nodeType;
    juce::Identifier property;
    std::optional<juce::String> defaultText;
};

const std::vector<PropertyRule>& propertyRules()
{
    static const std::vector<PropertyRule> rules {
        { ids::Event, ids::kind, "cc" },
        { ids::Note, "pitch", std::nullopt },
    };
    return rules;
}
