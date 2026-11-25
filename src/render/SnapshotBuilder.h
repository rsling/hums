#pragma once

#include "model/ClipModel.h"
#include "model/ProjectModel.h"
#include "model/TimelineModel.h"
#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/Snapshot.h"

#include <cstddef>
#include <vector>

namespace render
{

// Document -> Snapshot (docs/RENDERING.md -> Pipeline). The one place that
// reads the document for rendering: the render core never sees a ValueTree.
// Message thread only, like everything that reads the document.
//
// The document must be valid (model/Validator); what the Validator refuses
// is a programming error here and asserts.
class SnapshotBuilder
{
public:
    // The whole document: the Timeline's tempos, a template for every clip
    // that a placement (of a track or of an AutomationLane) or a slot
    // references, and a plan for every Track: its own placements and those
    // of each of its AutomationLanes, in the order of the lanes. A clip
    // nothing references gets no template.
    static Snapshot build (const model::ProjectModel& project);

    // What a clip plays in one pass of its loop.
    //
    // An event clip: its enabled notes and events with tick < loopLength;
    // content at or past the loop length is never played. Each list is put
    // in tick order, and what shares a tick stays in child order, which is
    // the tie-breaker of docs/RENDERING.md -> Equal-tick order. The clip's
    // quantise setting is applied here, once (model/Quantise): a note stands
    // at its quantised onset with its stored length, and that onset is its
    // position for the order within the template and for every later rule.
    // An event is never quantised.
    //
    // A step clip (docs/RENDERING.md -> StepClip): its Grid in time. The
    // columns before resetAt that are not skipped follow each other, each one
    // step length long; a skipped column takes no time, and what stands in
    // it or from resetAt on is not played. Every cell of such a column gives
    // its CC values as cc events at the column's start, an off cell as an on
    // cell, and an on cell a note there: the lane's fixed pitch, or the
    // cell's own where the lane has none, and round (gate x stepLength) ticks
    // long, an exact half going up, 1 tick at least. That length is counted
    // on from the column's start, so a gate above 1 reaches into the audible
    // columns that follow, and past the last of them into the next pass of
    // the loop. The note-off has velocity 64: a cell stores none. What
    // shares a tick is in the order of the lanes, and within a cell in the
    // order of its CC values; the Kernel plays a tick's events before its
    // notes, so a column's CC values come before its note-ons.
    //
    // The tie: where on cells of one pitch stand in audible columns that
    // follow each other in a lane, and the earlier one's note is a step long
    // or longer, so that it reaches the later column's start, the two are one
    // note. It begins with the earlier cell and its velocity, and ends where
    // the later cell's note would have ended. A third cell is tied on in the
    // same way. A note shorter than a step does not reach, and the next cell
    // begins a note of its own; so does a cell after an off cell. Nothing is
    // tied across the loop point [D3]: the last column's note plays on into
    // the next pass like any note that is longer than the rest of the loop,
    // and the first column begins its own note there.
    //
    // The chase tables (docs/RENDERING.md -> Chase) are made from the
    // template's events, so they hold what is played and nothing else: the
    // cc events by controller and the program changes, of an event clip, and
    // of a step clip the CC values of its cells.
    //
    // The template's `controllers` are those it has a chase table for. Where
    // the clip is on an AutomationLane they are what its placements own
    // (docs/RENDERING.md -> AutomationLane merge): the controllers of its
    // enabled cc events with tick < loopLength, and no others.
    static ClipTemplate templateOf (const model::ClipModel& clip);

    // The Timeline's tempos in tick order, each as the whole microseconds
    // per quarter note it is played and exported as [D13]: what a TempoMap
    // is made from, and what SMF export writes into the conductor track.
    static std::vector<TempoMap::Segment> tempoSegmentsOf (const model::TimelineModel& timeline);

    // Into which destination each Track plays, as render::NoteTracker numbers
    // them.
    struct Destinations
    {
        // One per Track, in track order: 0 to count - 1.
        std::vector<std::size_t> destinationOfTrack;
        std::size_t count = 0;
    };

    // The destinations where there is no hardware: for SMF export and the
    // golden tests (docs/RENDERING.md -> Same-pitch overlap, decided 25 Nov
    // 2026). The tracks of one Player share a destination, whether or not the
    // Player is bound to a port, and every track without a Player has one of
    // its own. The tracker keeps the channels of a destination apart itself.
    //
    // Live, the engine numbers its physical ports instead; this is not asked.
    static Destinations destinationsWithoutHardware (const model::ProjectModel& project);
};

} // namespace render
