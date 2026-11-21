#pragma once

#include "model/PlacementModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// A track-level override layer for CCs (docs/SCHEMA.md -> AutomationLanes /
// AutomationLane). Its placements reference event clips that hold only cc
// events.
class AutomationLaneModel
{
public:
    // The node must be an AutomationLane node.
    explicit AutomationLaneModel (juce::ValueTree automationLaneNode);

    juce::String getId() const;
    juce::String getName() const;

    // In document order, which is not necessarily time order. The Placement
    // nodes are direct children of the lane.
    std::vector<PlacementModel> getPlacements() const;

private:
    juce::ValueTree node;
};

} // namespace model
