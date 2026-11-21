#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// A reference to a clip at a position on a Track or on an AutomationLane
// (docs/SCHEMA.md -> Placements / Placement). The clip loops inside the
// placement; there is no stretch.
class PlacementModel
{
public:
    // The node must be a Placement node.
    explicit PlacementModel (juce::ValueTree placementNode);

    juce::String getId() const;
    juce::String getClipId() const;
    // Arrangement tick, >= 0.
    std::int64_t getStart() const;
    // Extent on the timeline in ticks, > 0.
    std::int64_t getLength() const;
    // Ticks into the clip's loop at which playback starts.
    std::int64_t getOffset() const;

    // Computed, not stored: start + length, the first tick after the
    // placement. The placement covers [start, end).
    std::int64_t getEnd() const;

private:
    juce::ValueTree node;
};

} // namespace model
