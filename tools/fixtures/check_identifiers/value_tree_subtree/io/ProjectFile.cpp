// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A subtree written as a list in braces is a ValueTree too, without the
// word: its type is a name, at any depth.
juce::ValueTree makeEmptyClip()
{
    return juce::ValueTree { model::ids::Clip,
                             { { model::ids::kind, "event" } },
                             { { "Events", {} } } };
}
