#include "model/TempoEventModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

TempoEventModel::TempoEventModel (juce::ValueTree tempoEventNode)
    : node (std::move (tempoEventNode))
{
    jassert (node.hasType (ids::TempoEvent));
}

std::int64_t TempoEventModel::getTick() const
{
    return PropertyReader::getRequiredInt64 (node, ids::tick);
}

double TempoEventModel::getBpm() const
{
    return PropertyReader::getRequiredDouble (node, ids::bpm);
}

} // namespace model
