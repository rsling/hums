#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// One pad of the PadMap (docs/SCHEMA.md -> Routing).
class PadModel
{
public:
    // The node must be a Pad node.
    explicit PadModel (juce::ValueTree padNode);

    // Pad number, 0-based, unique within the map.
    int getIndex() const;
    // The note the physical pad sends; how incoming MIDI identifies the pad.
    int getInputPitch() const;
    // The note the pad produces in a drum Lane.
    int getPitch() const;
    juce::String getLabel() const;

private:
    juce::ValueTree node;
};

} // namespace model
