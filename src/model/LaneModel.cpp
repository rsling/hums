#include "model/LaneModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

LaneModel::LaneModel (juce::ValueTree laneNode)
    : node (std::move (laneNode))
{
    jassert (node.hasType (ids::Lane));
}

juce::String LaneModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String LaneModel::getName() const
{
    return PropertyReader::getString (node, ids::name, "");
}

std::optional<int> LaneModel::getFixedPitch() const
{
    const auto fixedPitch = PropertyReader::readInt (node, ids::fixedPitch);

    // The Validator lets no document in whose fixedPitch is not a number.
    jassert (! fixedPitch.isMalformed());

    if (fixedPitch.isValid())
        return fixedPitch.value;

    return std::nullopt;
}

std::vector<CellModel> LaneModel::getCells() const
{
    std::vector<CellModel> cells;

    for (const auto& child : node)
        if (child.hasType (ids::Cell))
            cells.emplace_back (child);

    return cells;
}

} // namespace model
