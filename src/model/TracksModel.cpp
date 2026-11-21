#include "model/TracksModel.h"

#include "model/Ids.h"

#include <algorithm>
#include <utility>

namespace model
{

namespace
{

// Every placement under one track: its own, then those of its automation lanes.
    std::vector<PlacementModel> allPlacementsOf (const TrackModel& track)
    {
        auto placements = track.getPlacements();

        for (const auto& lane : track.getAutomationLanes())
            for (const auto& placement : lane.getPlacements())
                placements.push_back (placement);

        return placements;
    }

} // namespace

TracksModel::TracksModel (juce::ValueTree tracksNode)
    : node (std::move (tracksNode))
{
    jassert (node.hasType (ids::Tracks));
}

std::vector<TrackModel> TracksModel::getTracks() const
{
    std::vector<TrackModel> tracks;

    for (const auto& child : node)
        if (child.hasType (ids::Track))
            tracks.emplace_back (child);

    return tracks;
}

int TracksModel::getRefcount (const juce::String& clipId) const
{
    int refcount = 0;

    for (const auto& track : getTracks())
    {
        for (const auto& placement : allPlacementsOf (track))
            if (placement.getClipId() == clipId)
                ++refcount;

        for (const auto& slot : track.getSlots())
            if (slot.getClipId() == clipId)
                ++refcount;
    }

    return refcount;
}

std::optional<std::int64_t> TracksModel::getArrangementEnd() const
{
    std::optional<std::int64_t> arrangementEnd;

    for (const auto& track : getTracks())
        for (const auto& placement : allPlacementsOf (track))
            arrangementEnd = std::max (arrangementEnd.value_or (0), placement.getEnd());

    return arrangementEnd;
}

} // namespace model
