#pragma once

#include "model/ClipModel.h"
#include "model/ProjectModel.h"
#include "model/TimelineModel.h"
#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/Snapshot.h"

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
    // references, and a plan for every Track. A clip nothing references gets
    // no template.
    static Snapshot build (const model::ProjectModel& project);

    // What a clip plays in one pass of its loop.
    //
    // An event clip: its enabled notes and events with tick < loopLength;
    // content at or past the loop length is never played. Each list is put
    // in tick order, and what shares a tick stays in child order, which is
    // the tie-breaker of docs/RENDERING.md -> Equal-tick order. The clip's
    // quantise setting is not applied yet: a note stands at its stored tick.
    //
    // A step clip: its loop length only. Expanding the Grid into notes and
    // events is not written yet.
    static ClipTemplate templateOf (const model::ClipModel& clip);

    // The Timeline's tempos in tick order, each as the whole microseconds
    // per quarter note it is played and exported as [D13]: what a TempoMap
    // is made from, and what SMF export writes into the conductor track.
    static std::vector<TempoMap::Segment> tempoSegmentsOf (const model::TimelineModel& timeline);
};

} // namespace render
