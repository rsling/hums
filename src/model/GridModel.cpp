#include "model/GridModel.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <utility>

namespace model
{

GridModel::GridModel (juce::ValueTree gridNode)
    : node (std::move (gridNode))
{
    jassert (node.hasType (ids::Grid));
}

std::int64_t GridModel::getStepLength() const
{
    return PropertyReader::getInt64 (node, ids::stepLength, 240);
}

int GridModel::getStepCount() const
{
    return PropertyReader::getInt (node, ids::stepCount, 16);
}

int GridModel::getResetAt() const
{
    return PropertyReader::getInt (node, ids::resetAt, getStepCount());
}

juce::String GridModel::getSkipMask() const
{
    const auto stored = PropertyReader::readString (node, ids::skipMask);

    if (stored.isValid())
        return stored.value;

    return juce::String::repeatedString ("0", getStepCount());
}

bool GridModel::isColumnSkipped (int column) const
{
    const auto skipMask = getSkipMask();

    jassert (column >= 0 && column < skipMask.length());

    return skipMask[column] == '1';
}

int GridModel::getAudibleColumnCount() const
{
    const auto skipMask = getSkipMask();
    const auto resetAt = getResetAt();

    int audibleColumns = 0;

    for (int column = 0; column < resetAt; ++column)
        if (skipMask[column] != '1')
            ++audibleColumns;

    return audibleColumns;
}

std::int64_t GridModel::getLoopLength() const
{
    return getStepLength() * getAudibleColumnCount();
}

std::vector<LaneModel> GridModel::getLanes() const
{
    std::vector<LaneModel> lanes;

    for (const auto& child : node)
        if (child.hasType (ids::Lane))
            lanes.emplace_back (child);

    return lanes;
}

} // namespace model
