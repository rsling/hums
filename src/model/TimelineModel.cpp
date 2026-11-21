#include "model/TimelineModel.h"

#include "model/Ids.h"

#include <utility>

namespace model
{

TimelineModel::TimelineModel (juce::ValueTree timelineNode)
    : node (std::move (timelineNode))
{
    jassert (node.hasType (ids::Timeline));
}

std::vector<TempoEventModel> TimelineModel::getTempoEvents() const
{
    std::vector<TempoEventModel> tempoEvents;

    for (const auto& child : node)
        if (child.hasType (ids::TempoEvent))
            tempoEvents.emplace_back (child);

    return tempoEvents;
}

std::vector<TimeSigEventModel> TimelineModel::getTimeSigEvents() const
{
    std::vector<TimeSigEventModel> timeSigEvents;

    for (const auto& child : node)
        if (child.hasType (ids::TimeSigEvent))
            timeSigEvents.emplace_back (child);

    return timeSigEvents;
}

std::vector<MarkerModel> TimelineModel::getMarkers() const
{
    std::vector<MarkerModel> markers;

    for (const auto& child : node)
        if (child.hasType (ids::Marker))
            markers.emplace_back (child);

    return markers;
}

} // namespace model
