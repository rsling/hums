#include "model/TrackModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

TrackModel::TrackModel (juce::ValueTree trackNode)
    : node (std::move (trackNode))
{
    jassert (node.hasType (ids::Track));
}

juce::String TrackModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String TrackModel::getName() const
{
    return PropertyReader::getString (node, ids::name, "");
}

juce::String TrackModel::getColour() const
{
    return PropertyReader::getString (node, ids::colour, "");
}

juce::String TrackModel::getPlayerId() const
{
    return PropertyReader::getString (node, ids::playerId, "");
}

int TrackModel::getChannel() const
{
    return PropertyReader::getInt (node, ids::channel, 1);
}

juce::String TrackModel::getPerformerId() const
{
    return PropertyReader::getString (node, ids::performerId, "");
}

bool TrackModel::isMuted() const
{
    return PropertyReader::getBool (node, ids::mute, false);
}

bool TrackModel::isSoloed() const
{
    return PropertyReader::getBool (node, ids::solo, false);
}

std::vector<PlacementModel> TrackModel::getPlacements() const
{
    std::vector<PlacementModel> placements;

    for (const auto& child : node.getChildWithName (ids::Placements))
        if (child.hasType (ids::Placement))
            placements.emplace_back (child);

    return placements;
}

std::vector<AutomationLaneModel> TrackModel::getAutomationLanes() const
{
    std::vector<AutomationLaneModel> lanes;

    for (const auto& child : node.getChildWithName (ids::AutomationLanes))
        if (child.hasType (ids::AutomationLane))
            lanes.emplace_back (child);

    return lanes;
}

std::vector<KnobModel> TrackModel::getKnobs() const
{
    std::vector<KnobModel> knobs;

    for (const auto& child : node.getChildWithName (ids::Knobs))
        if (child.hasType (ids::Knob))
            knobs.emplace_back (child);

    return knobs;
}

std::vector<SlotModel> TrackModel::getSlots() const
{
    std::vector<SlotModel> slots;

    for (const auto& child : node.getChildWithName (ids::Slots))
        if (child.hasType (ids::Slot))
            slots.emplace_back (child);

    return slots;
}

} // namespace model
