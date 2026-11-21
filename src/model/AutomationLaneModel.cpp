#include "model/AutomationLaneModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

AutomationLaneModel::AutomationLaneModel (juce::ValueTree automationLaneNode)
    : node (std::move (automationLaneNode))
{
    jassert (node.hasType (ids::AutomationLane));
}

juce::String AutomationLaneModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String AutomationLaneModel::getName() const
{
    return PropertyReader::getString (node, ids::name, "");
}

std::vector<PlacementModel> AutomationLaneModel::getPlacements() const
{
    std::vector<PlacementModel> placements;

    for (const auto& child : node)
        if (child.hasType (ids::Placement))
            placements.emplace_back (child);

    return placements;
}

} // namespace model
