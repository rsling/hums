// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name in a plain array of structs of ours.
struct RangeRule
{
    juce::Identifier property;
    int lowest = 0;
    int highest = 127;
};

const RangeRule rangeRules[] = {
    { ids::pitch, 0, 127 },
    { "velocity", 1, 127 },
};
