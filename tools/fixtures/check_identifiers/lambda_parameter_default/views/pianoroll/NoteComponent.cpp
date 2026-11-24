// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The parameter of a lambda is a juce::Identifier, and its default is a
// literal: a name. The default of the second lambda's parameter is a text
// and no hit, although "pitch" is a schema name.
void NoteComponent::refresh()
{
    const auto valueOf = [this] (const juce::Identifier& property = "pitch") { return note.getProperty (property); };
    const auto labelFor = [] (const juce::String& label = "pitch") { return label.toUpperCase(); };
    setTooltip (labelFor() + valueOf().toString());
}
