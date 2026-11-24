// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: the brace in '{' is a character, not code, so the body of
// typeAfter ends where it ends. Its return is the hit. label() returns a
// juce::String, and "Clip" there is text and no hit, although it is also a
// schema name.
juce::Identifier typeAfter (juce::juce_wchar first)
{
    if (first == '{')
        return model::ids::Clip;

    return "Project";
}

juce::String label()
{
    return "Clip";
}
