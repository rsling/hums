#include "model/TimeSigEventModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

TimeSigEventModel::TimeSigEventModel (juce::ValueTree timeSigEventNode)
    : node (std::move (timeSigEventNode))
{
    jassert (node.hasType (ids::TimeSigEvent));
}

std::int64_t TimeSigEventModel::getTick() const
{
    return PropertyReader::getRequiredInt64 (node, ids::tick);
}

int TimeSigEventModel::getNumerator() const
{
    return PropertyReader::getRequiredInt (node, ids::numerator);
}

int TimeSigEventModel::getDenominator() const
{
    return PropertyReader::getRequiredInt (node, ids::denominator);
}

} // namespace model
