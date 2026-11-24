// Fixture for tools/check_identifiers.py: this tree must fail the check.
// "swing" is not a schema name, and that is the point: in io/ a property
// that has no constant in Ids.h is an invented one, also in a table of rules.
struct PropertyRule
{
    juce::Identifier nodeType;
    juce::Identifier property;
    std::optional<juce::String> defaultText;
};

static const std::vector<PropertyRule> rules {
    { ids::Cell, ids::pitch, "60" },
    { ids::Cell, "swing", "0" },
};
