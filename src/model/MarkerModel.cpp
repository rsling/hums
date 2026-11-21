#include "model/MarkerModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

MarkerModel::MarkerModel (juce::ValueTree markerNode)
    : node (std::move (markerNode))
{
    jassert (node.hasType (ids::Marker));
}

std::int64_t MarkerModel::getTick() const
{
    return PropertyReader::getRequiredInt64 (node, ids::tick);
}

juce::String MarkerModel::getName() const
{
    return PropertyReader::getRequiredString (node, ids::name);
}

} // namespace model
