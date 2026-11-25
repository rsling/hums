#pragma once

#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/TrackPlan.h"

#include <vector>

namespace render
{

// The document as the Kernel plays it: immutable plain data, made by
// SnapshotBuilder on the message thread after an edit and read by the engine
// and by SMF export (docs/RENDERING.md -> Pipeline). Not a flat event list:
// the Kernel expands it over a window of ticks.
//
// It carries no mute or solo, no transport loop and no knob values; those
// reach the engine as live commands (docs/ARCHITECTURE.md -> Threads).
//
// Std only, and fixed in size once built: reading it allocates nothing.
struct Snapshot
{
    // The Timeline's tempos; the one authority for tick <-> time.
    TempoMap tempoMap;

    // One per clip that a placement or a slot references, in the order of
    // the ClipPool. A TrackPlan names a template by its position here.
    std::vector<ClipTemplate> clipTemplates;

    // One per Track, in track order.
    std::vector<TrackPlan> trackPlans;
};

} // namespace render
