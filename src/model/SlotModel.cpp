#include "model/SlotModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

SlotModel::SlotModel (juce::ValueTree slotNode)
    : node (std::move (slotNode))
{
    jassert (node.hasType (ids::Slot));
}

int SlotModel::getScene() const
{
    return PropertyReader::getRequiredInt (node, ids::scene);
}

juce::String SlotModel::getClipId() const
{
    return PropertyReader::getRequiredString (node, ids::clipId);
}

} // namespace model
