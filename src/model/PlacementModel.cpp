#include "model/PlacementModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

PlacementModel::PlacementModel (juce::ValueTree placementNode)
    : node (std::move (placementNode))
{
    jassert (node.hasType (ids::Placement));
}

juce::String PlacementModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String PlacementModel::getClipId() const
{
    return PropertyReader::getRequiredString (node, ids::clipId);
}

std::int64_t PlacementModel::getStart() const
{
    return PropertyReader::getRequiredInt64 (node, ids::start);
}

std::int64_t PlacementModel::getLength() const
{
    return PropertyReader::getRequiredInt64 (node, ids::length);
}

std::int64_t PlacementModel::getOffset() const
{
    return PropertyReader::getInt64 (node, ids::offset, 0);
}

std::int64_t PlacementModel::getEnd() const
{
    return getStart() + getLength();
}

} // namespace model
