// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The first two declarations open like the construction of a variable,
// "juce::ValueTree name (...)", but what stands in their brackets is a
// parameter with a default text. A text is no name, also when it equals a
// schema name: no hit. The parameter of the third is a juce::Identifier, and
// its default is a name: the one hit, in line 10.
juce::ValueTree findNodeLabelled (const juce::String& label = "pitch");
juce::Identifier typeShownFor (int depth, const juce::String& label = "untitled");

juce::Identifier typeOrFallback (const juce::ValueTree& node, const juce::Identifier& fallback = "Clip");
