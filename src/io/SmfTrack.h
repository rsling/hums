#pragma once

#include "io/SmfEvent.h"

#include <cstdint>
#include <vector>

namespace io
{

// One MTrk chunk of a Standard MIDI File: its events in the order in which
// they stand in the file. That order is never changed, not by the reader and
// not by the writer; at one tick it is what the file says about which event
// comes first.
//
// Std only, a plain value.
struct SmfTrack
{
    // Ticks never decrease from one event to the next.
    std::vector<SmfEvent> events;

    // The tick of the End of Track meta event, at or after the last event.
    // Every track has one, and it is kept here and not among the events.
    std::int64_t endTick = 0;

    bool operator== (const SmfTrack&) const = default;
};

} // namespace io
