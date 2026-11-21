#include "model/PadModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

PadModel::PadModel (juce::ValueTree padNode)
    : node (std::move (padNode))
{
    jassert (node.hasType (ids::Pad));
}

int PadModel::getIndex() const
{
    return PropertyReader::getRequiredInt (node, ids::index);
}

int PadModel::getInputPitch() const
{
    return PropertyReader::getRequiredInt (node, ids::inputPitch);
}

int PadModel::getPitch() const
{
    return PropertyReader::getRequiredInt (node, ids::pitch);
}

juce::String PadModel::getLabel() const
{
    return PropertyReader::getString (node, ids::label, "");
}

} // namespace model
