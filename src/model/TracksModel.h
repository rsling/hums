#pragma once

#include "model/TrackModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace model
{

// The project's tracks (docs/SCHEMA.md -> Tracks / Track). Every reference
// to a clip lives somewhere under this node, so the two values that are
// computed from all references are computed here: a clip's refcount and the
// arrangement end. Neither is ever stored.
class TracksModel
{
public:
    // The node must be a Tracks node.
    explicit TracksModel (juce::ValueTree tracksNode);

    // In document order: top to bottom.
    std::vector<TrackModel> getTracks() const;

    // The number of Placements and Slots that reference this clip, on every
    // track: the tracks' own placements, those of their AutomationLanes, and
    // their slots. 0 for a clip nothing references, and for an id no clip has.
    int getRefcount (const juce::String& clipId) const;

    // The largest start + length over every Placement on every Track and
    // AutomationLane (docs/SCHEMA.md -> UiState). Slots never count. Nothing
    // when there is no placement at all: then there is no arrangement, which
    // is not the same as one that ends at tick 0.
    std::optional<std::int64_t> getArrangementEnd() const;

private:
    juce::ValueTree node;
};

} // namespace model
