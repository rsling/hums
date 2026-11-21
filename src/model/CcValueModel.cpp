#include "model/CcValueModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

CcValueModel::CcValueModel (juce::ValueTree ccValueNode)
    : node (std::move (ccValueNode))
{
    jassert (node.hasType (ids::CcValue));
}

int CcValueModel::getCc() const
{
    return PropertyReader::getRequiredInt (node, ids::cc);
}

int CcValueModel::getValue() const
{
    return PropertyReader::getRequiredInt (node, ids::value);
}

} // namespace model
