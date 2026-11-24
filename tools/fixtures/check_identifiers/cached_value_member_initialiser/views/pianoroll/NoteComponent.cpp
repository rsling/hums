// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A juce::CachedValue that is a member, declared in NoteComponent.h, is given
// its tree and its name where the constructor initialises it.
NoteComponent::NoteComponent (juce::ValueTree& note)
    : velocity (note, "velocity", nullptr)
{
}
