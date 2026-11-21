#include "model/PerformerModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

PerformerModel::PerformerModel (juce::ValueTree performerNode)
    : node (std::move (performerNode))
{
    jassert (node.hasType (ids::Performer));
}

juce::String PerformerModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String PerformerModel::getName() const
{
    return PropertyReader::getRequiredString (node, ids::name);
}

juce::String PerformerModel::getDeviceIdentifier() const
{
    return PropertyReader::getString (node, ids::deviceIdentifier, "");
}

juce::String PerformerModel::getDeviceName() const
{
    return PropertyReader::getString (node, ids::deviceName, "");
}

int PerformerModel::getChannelFilter() const
{
    return PropertyReader::getInt (node, ids::channelFilter, 0);
}

} // namespace model
