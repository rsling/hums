#include "model/PlayerModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

PlayerModel::PlayerModel (juce::ValueTree playerNode)
    : node (std::move (playerNode))
{
    jassert (node.hasType (ids::Player));
}

juce::String PlayerModel::getId() const
{
    return PropertyReader::getRequiredString (node, ids::id);
}

juce::String PlayerModel::getName() const
{
    return PropertyReader::getRequiredString (node, ids::name);
}

juce::String PlayerModel::getDeviceIdentifier() const
{
    return PropertyReader::getString (node, ids::deviceIdentifier, "");
}

juce::String PlayerModel::getDeviceName() const
{
    return PropertyReader::getString (node, ids::deviceName, "");
}

bool PlayerModel::getSendClock() const
{
    return PropertyReader::getBool (node, ids::sendClock, false);
}

} // namespace model
