// Fixture for tools/check_identifiers.py: the member that the constructor in
// NoteComponent.cpp initialises. Nothing here is a hit.
class NoteComponent
{
public:
    explicit NoteComponent (juce::ValueTree& note);

private:
    juce::CachedValue<int> velocity;
};
