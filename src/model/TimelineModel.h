#pragma once

#include "model/MarkerModel.h"
#include "model/TempoEventModel.h"
#include "model/TimeSigEventModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model
{

// The global tempo map, time-signature map and markers (docs/SCHEMA.md ->
// Timeline): the document side of what an SMF holds in its conductor track.
//
// This wrapper only reads what is stored. It does not sort and does not
// convert between ticks and time: render/TempoMap is the one authority for
// that.
class TimelineModel
{
public:
    // The node must be a Timeline node.
    explicit TimelineModel (juce::ValueTree timelineNode);

    // In document order, which is not necessarily tick order.
    std::vector<TempoEventModel> getTempoEvents() const;
    // In document order, which is not necessarily tick order.
    std::vector<TimeSigEventModel> getTimeSigEvents() const;
    // In document order, which is not necessarily tick order.
    std::vector<MarkerModel> getMarkers() const;

private:
    juce::ValueTree node;
};

} // namespace model
