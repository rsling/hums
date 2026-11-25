#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace render
{

// What one Track plays in the arrangement, as plain data (docs/RENDERING.md
// -> Per track, AutomationLane merge): its MIDI channel, its placements and
// those of its AutomationLanes. A Snapshot holds one plan per Track, in track
// order.
//
// A plan says nothing about the track's Player: rendering is independent of
// routing. It holds no mute and no solo either; those reach the engine as
// live commands.
//
// Std only, and fixed in size once built.
struct TrackPlan
{
    struct Placement
    {
        // The placement covers the arrangement ticks [start, start + length).
        std::int64_t start = 0;
        std::int64_t length = 0;
        // Ticks into the clip's loop at which playback starts,
        // 0 <= offset < the template's loopLength.
        std::int64_t offset = 0;
        // Which of the Snapshot's clipTemplates loops inside the placement.
        std::size_t templateIndex = 0;
    };

    // 1 to 16.
    int channel = 1;

    // The track's own placements in the order of their starts. No two share
    // a tick, so one ends before or where the next begins. They stand here
    // as the document has them: the Kernel plays those that continue each
    // other without a break as one.
    std::vector<Placement> placements;

    // One AutomationLane of the track: its placements in the order of their
    // starts, none sharing a tick with another. Each names the template of a
    // clip that holds cc events only; the template's `controllers` are what
    // the placement owns.
    struct AutomationLane
    {
        std::vector<Placement> placements;
    };

    // The track's AutomationLanes in their own order, which says who wins:
    // where two own a controller at one tick, the later lane has it.
    std::vector<AutomationLane> automationLanes;
};

} // namespace render
