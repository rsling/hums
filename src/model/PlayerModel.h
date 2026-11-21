#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// A logical MIDI output (docs/SCHEMA.md -> Routing). Persisted by name and
// bound to a physical device at runtime. It has no channel: the channel
// belongs to the Track.
class PlayerModel
{
public:
    // The node must be a Player node.
    explicit PlayerModel (juce::ValueTree playerNode);

    juce::String getId() const;
    juce::String getName() const;
    // The juce::MidiDeviceInfo::identifier it was last bound to; empty if never bound.
    juce::String getDeviceIdentifier() const;
    // The fallback match when the identifier is gone.
    juce::String getDeviceName() const;
    // MIDI clock, Start / Stop / Continue and Song Position Pointer go to this Player's port.
    bool getSendClock() const;

private:
    juce::ValueTree node;
};

} // namespace model
