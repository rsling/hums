// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A reference to a juce::Identifier, bound to a literal in braces. The
// declaration before it returns such a reference and constructs nothing: the
// default of its parameter is a text, and no hit. Nor is the text in the
// body of the lambda, whose brace opens after "-> const juce::Identifier&".
const juce::Identifier& typeShownFor (const juce::String& label = "untitled");

bool isClip (const juce::ValueTree& node)
{
    const auto typeOf = [] (const juce::ValueTree& asked) -> const juce::Identifier& {
        juce::Logger::writeToLog ("asked for a type");
        return typeShownFor (asked.getType().toString());
    };
    const juce::Identifier& clipType { "Clip" };
    return typeOf (node) == clipType;
}
