#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// One knob assignment of a Track (docs/SCHEMA.md -> Knobs / Knob): the
// controller number the knob sends and records, and what it is called.
class KnobModel
{
public:
    // The node must be a Knob node.
    explicit KnobModel (juce::ValueTree knobNode);

    // Controller number, 0-127, unique within the track.
    int getCc() const;
    juce::String getLabel() const;

private:
    juce::ValueTree node;
};

} // namespace model
