#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// A named position on the Timeline (docs/SCHEMA.md -> Timeline).
class MarkerModel
{
public:
    // The node must be a Marker node.
    explicit MarkerModel (juce::ValueTree markerNode);

    std::int64_t getTick() const;
    // Empty when the node has no name [D16].
    juce::String getName() const;

private:
    juce::ValueTree node;
};

} // namespace model
