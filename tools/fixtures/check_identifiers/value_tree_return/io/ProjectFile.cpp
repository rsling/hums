// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: a function that returns a ValueTree constructs one from the
// list in braces it returns. "tick" is the hit; "cc", the value of kind, is
// none, although it is also a schema name.
juce::ValueTree makeCcEvent (juce::int64 tick)
{
    return { model::ids::Event,
             { { model::ids::kind, "cc" },
               { "tick", tick } } };
}
