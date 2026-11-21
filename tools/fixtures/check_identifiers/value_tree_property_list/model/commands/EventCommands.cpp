// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: in a ValueTree constructed with its properties, the first
// half of each pair is a name and the second a value. "tick" is the hit;
// "cc", the value of kind, is none, although it is also a schema name.
juce::ValueTree makeCcEvent (juce::int64 tick)
{
    return juce::ValueTree { model::ids::Event,
                             { { model::ids::kind, "cc" },
                               { "tick", tick } } };
}
