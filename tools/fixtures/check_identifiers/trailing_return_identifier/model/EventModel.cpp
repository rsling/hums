// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: the brace after "-> juce::Identifier" opens the body of the
// lambda, not the brackets of an Identifier construction, so "cc" in the
// comparison is no hit. What the lambda returns is a name: "kind" is the hit.
static const auto propertyFor = [] (const juce::String& word) -> juce::Identifier {
    if (word == "cc")
        return model::ids::cc;
    return "kind";
};
