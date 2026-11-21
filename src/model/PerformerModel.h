#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// A logical MIDI input (docs/SCHEMA.md -> Routing). Persisted by name and
// bound to a physical device at runtime: by deviceIdentifier, then by
// deviceName, else unassigned.
class PerformerModel
{
public:
    // The node must be a Performer node.
    explicit PerformerModel (juce::ValueTree performerNode);

    juce::String getId() const;
    // What the user sees everywhere.
    juce::String getName() const;
    // The juce::MidiDeviceInfo::identifier it was last bound to; empty if never bound.
    juce::String getDeviceIdentifier() const;
    // The fallback match when the identifier is gone.
    juce::String getDeviceName() const;
    // 0 = omni, 1-16 = only that channel.
    int getChannelFilter() const;

private:
    juce::ValueTree node;
};

} // namespace model
