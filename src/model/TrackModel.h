#pragma once

#include "model/AutomationLaneModel.h"
#include "model/KnobModel.h"
#include "model/PlacementModel.h"
#include "model/SlotModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// A lane in the arrangement (docs/SCHEMA.md -> Tracks / Track). It has one
// Player and one MIDI channel, and owns Placements, AutomationLanes, Knobs
// and Slots.
class TrackModel
{
public:
    // The node must be a Track node.
    explicit TrackModel (juce::ValueTree trackNode);

    juce::String getId() const;
    juce::String getName() const;
    // "#AARRGGBB", or empty for none.
    juce::String getColour() const;
    // Empty = unassigned. The track still renders and exports; only live
    // transmission needs a Player.
    juce::String getPlayerId() const;
    // 1-16.
    int getChannel() const;
    // The record input; empty = not recordable.
    juce::String getPerformerId() const;
    bool isMuted() const;
    bool isSoloed() const;

    // The track's own placements, in document order, which is not
    // necessarily time order. Those of its AutomationLanes are not included.
    std::vector<PlacementModel> getPlacements() const;
    // In document order: a later lane overrides an earlier one.
    std::vector<AutomationLaneModel> getAutomationLanes() const;
    // In document order: left to right.
    std::vector<KnobModel> getKnobs() const;
    // In document order, which is not necessarily scene order. Only occupied
    // slots are stored.
    std::vector<SlotModel> getSlots() const;

private:
    juce::ValueTree node;
};

} // namespace model
