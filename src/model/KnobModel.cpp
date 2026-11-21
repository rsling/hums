#include "model/KnobModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

KnobModel::KnobModel (juce::ValueTree knobNode)
    : node (std::move (knobNode))
{
    jassert (node.hasType (ids::Knob));
}

int KnobModel::getCc() const
{
    return PropertyReader::getRequiredInt (node, ids::cc);
}

juce::String KnobModel::getLabel() const
{
    return PropertyReader::getString (node, ids::label, "");
}

} // namespace model
