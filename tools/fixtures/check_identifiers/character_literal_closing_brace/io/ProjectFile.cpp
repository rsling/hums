// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The brace in '}' is a character, not code: it does not close the body of
// the function, and the return after it is still a return of an Identifier.
juce::Identifier typeAfter (juce::juce_wchar last)
{
    if (last == '}')
        return model::ids::Clip;

    return "Project";
}
