#include "render/SnapshotBuilder.h"

#include "model/CellModel.h"
#include "model/EventModel.h"
#include "model/GridModel.h"
#include "model/LaneModel.h"
#include "model/NoteModel.h"
#include "model/PlacementModel.h"
#include "model/Quantise.h"
#include "model/TrackModel.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>

namespace render
{

namespace
{

    ClipTemplate::EventKind kindOf (model::EventModel::Kind kind)
    {
        switch (kind)
        {
            case model::EventModel::Kind::cc:
                return ClipTemplate::EventKind::cc;
            case model::EventModel::Kind::pitchBend:
                return ClipTemplate::EventKind::pitchBend;
            case model::EventModel::Kind::programChange:
                return ClipTemplate::EventKind::programChange;
            case model::EventModel::Kind::channelPressure:
                return ClipTemplate::EventKind::channelPressure;
            case model::EventModel::Kind::polyPressure:
                return ClipTemplate::EventKind::polyPressure;
            case model::EventModel::Kind::sysex:
                return ClipTemplate::EventKind::sysex;
        }

        jassertfalse;
        return ClipTemplate::EventKind::cc;
    }

// Appends the bytes that a sysex event's hexadecimal digits stand for. The
// Validator lets no document in whose payload is anything but pairs of such
// digits (invariant 10).
    void appendBytesOfHex (std::vector<std::uint8_t>& bytes, const juce::String& hexDigits)
    {
        jassert (hexDigits.length() % 2 == 0);

        auto digit = hexDigits.getCharPointer();

        while (! digit.isEmpty())
        {
            const int high = juce::CharacterFunctions::getHexDigitValue (digit.getAndAdvance());
            const int low = juce::CharacterFunctions::getHexDigitValue (digit.getAndAdvance());
            jassert (high >= 0 && low >= 0);

            bytes.push_back (static_cast<std::uint8_t> (high * 16 + low));
        }
    }

// Every clip id that a placement of a track, a placement of one of its
// AutomationLanes, or a slot names.
    std::set<juce::String> referencedClipIdsOf (const std::vector<model::TrackModel>& tracks)
    {
        std::set<juce::String> clipIds;

        for (const auto& track : tracks)
        {
            for (const auto& placement : track.getPlacements())
                clipIds.insert (placement.getClipId());

            for (const auto& lane : track.getAutomationLanes())
                for (const auto& placement : lane.getPlacements())
                    clipIds.insert (placement.getClipId());

            for (const auto& slot : track.getSlots())
                clipIds.insert (slot.getClipId());
        }

        return clipIds;
    }

    TrackPlan planOf (const model::TrackModel& track, const std::map<juce::String, std::size_t>& templateIndexOfClipId)
    {
        TrackPlan plan;
        plan.channel = track.getChannel();

        for (const auto& placement : track.getPlacements())
        {
            const auto templateIndex = templateIndexOfClipId.find (placement.getClipId());

        // Invariant 1: every clipId resolves to a clip.
            jassert (templateIndex != templateIndexOfClipId.end());

            if (templateIndex == templateIndexOfClipId.end())
                continue;

            plan.placements.push_back ({ placement.getStart(), placement.getLength(), placement.getOffset(), templateIndex->second });
        }

    // The document keeps placements in the order they were made. No two of
    // one track share a tick (invariant 3), so their starts all differ.
        std::sort (plan.placements.begin(), plan.placements.end(), [] (const TrackPlan::Placement& earlier, const TrackPlan::Placement& later) {
            return earlier.start < later.start;
        });

        return plan;
    }

// The notes and events of an event clip, in child order: what is enabled
// and begins inside the loop, a note at its quantised onset.
    void addContentOfEventClip (ClipTemplate& clipTemplate, const model::ClipModel& clip)
    {
        const std::int64_t quantiseGrid = clip.getQuantiseGrid();

        for (const auto& note : clip.getNotes())
        {
            if (! note.isEnabled() || note.getTick() >= clipTemplate.loopLength)
                continue;

            // From here on the quantised onset is the note's position; the
            // stored tick is editing data. The length is kept.
            const std::int64_t onset = model::Quantise::onsetOf (note.getTick(), quantiseGrid, clipTemplate.loopLength);

            clipTemplate.notes.push_back ({ onset, note.getLength(), note.getPitch(), note.getVelocity(), note.getOffVelocity() });
        }

        for (const auto& event : clip.getEvents())
        {
            if (! event.isEnabled() || event.getTick() >= clipTemplate.loopLength)
                continue;

            ClipTemplate::Event templateEvent;
            templateEvent.tick = event.getTick();
            templateEvent.kind = kindOf (event.getKind());
            templateEvent.a = event.getA();
            templateEvent.b = event.getB();

            if (templateEvent.kind == ClipTemplate::EventKind::sysex)
            {
                templateEvent.dataStart = clipTemplate.sysexBytes.size();
                appendBytesOfHex (clipTemplate.sysexBytes, event.getData());
                templateEvent.dataLength = clipTemplate.sysexBytes.size() - templateEvent.dataStart;
            }

            clipTemplate.events.push_back (templateEvent);
        }
    }

// A column of a Grid that takes time, and the tick of the loop at which it
// begins.
    struct AudibleColumn
    {
        int column = 0;
        std::int64_t startTick = 0;
    };

// The columns of a Grid that are played in one pass of the loop, in order:
// those before resetAt that are not skipped. A skipped column takes no time,
// so each audible column begins where the audible one before it ends.
    std::vector<AudibleColumn> audibleColumnsOf (const model::GridModel& grid)
    {
        const std::int64_t stepLength = grid.getStepLength();
        const int resetAt = grid.getResetAt();

        std::vector<AudibleColumn> audibleColumns;

        for (int column = 0; column < resetAt; ++column)
        {
            if (grid.isColumnSkipped (column))
                continue;

            audibleColumns.push_back ({ column, stepLength * static_cast<std::int64_t> (audibleColumns.size()) });
        }

        return audibleColumns;
    }

// How long the note of a cell sounds: round (gate x stepLength) ticks, an
// exact half going up, and 1 tick at least. In audible time: the ticks are
// counted on from the cell's start, and a skipped column is not among them.
    std::int64_t noteLengthOf (double gate, std::int64_t stepLength)
    {
        return std::max<std::int64_t> (1, std::llround (gate * static_cast<double> (stepLength)));
    }

// A cell has no off velocity; its note-off goes out with the one a Note has
// when none is stored (docs/SCHEMA.md -> Note).
    constexpr int offVelocityOfStepNote = 64;

// The notes and events of a step clip (docs/RENDERING.md -> StepClip), lane
// by lane: for every audible column the cell's CC values at the column's
// start, of an off cell as of an on cell, and the note of an on cell.
//
// The tie: an on cell whose audible column follows directly on that of an
// on cell of the same pitch, and which the earlier note reaches, begins no
// note of its own. The earlier note is made to end where this cell's would
// have ended. A note reaches the next column when it is a step long or
// longer (decided 25 Nov 2026: a gate of exactly 1 ties). The columns are
// those of one pass of the loop, so the last one is never tied to the first
// [D3].
    void addContentOfStepClip (ClipTemplate& clipTemplate, const model::GridModel& grid)
    {
        const std::int64_t stepLength = grid.getStepLength();
        const auto audibleColumns = audibleColumnsOf (grid);

        for (const auto& lane : grid.getLanes())
        {
            const auto cells = lane.getCells();
            const auto fixedPitch = lane.getFixedPitch();

            // Invariant 2: every lane has a cell for every column.
            jassert (static_cast<int> (cells.size()) == grid.getStepCount());

            // Where in clipTemplate.notes the note of the lane's cell in the
            // audible column before this one stands; none when that cell is
            // off, and in the first column.
            std::optional<std::size_t> noteOfColumnBefore;

            for (const auto& audibleColumn : audibleColumns)
            {
                const auto& cell = cells[static_cast<std::size_t> (audibleColumn.column)];

                for (const auto& ccValue : cell.getCcValues())
                {
                    ClipTemplate::Event templateEvent;
                    templateEvent.tick = audibleColumn.startTick;
                    templateEvent.kind = ClipTemplate::EventKind::cc;
                    templateEvent.a = ccValue.getCc();
                    templateEvent.b = ccValue.getValue();

                    clipTemplate.events.push_back (templateEvent);
                }

                if (! cell.isOn())
                {
                    noteOfColumnBefore.reset();
                    continue;
                }

                // A lane with a fixed pitch plays it in every cell; the
                // cell's own pitch is kept and not played.
                const int pitch = fixedPitch.value_or (cell.getPitch());
                const std::int64_t length = noteLengthOf (cell.getGate(), stepLength);

                if (noteOfColumnBefore.has_value())
                {
                    auto& noteBefore = clipTemplate.notes[*noteOfColumnBefore];

                    if (noteBefore.pitch == pitch && noteBefore.tick + noteBefore.length >= audibleColumn.startTick)
                    {
                        // Tied: one note-on, the earlier cell's, so this
                        // cell's velocity is not heard; its gate says where
                        // the note ends, also when that is earlier than the
                        // note would have ended without it.
                        noteBefore.length = audibleColumn.startTick + length - noteBefore.tick;
                        continue;
                    }
                }

                noteOfColumnBefore = clipTemplate.notes.size();
                clipTemplate.notes.push_back ({ audibleColumn.startTick, length, pitch, cell.getVelocity(), offVelocityOfStepNote });
            }
        }
    }

// The chase tables of a template (docs/RENDERING.md -> Chase), from its
// events as they stand: in tick order, and in child order at one tick. Each
// table is therefore in that order too, and of two values at one tick the
// later child's is the later one.
    void addChaseTables (ClipTemplate& clipTemplate)
    {
        std::map<int, std::vector<ClipTemplate::ChasePoint>> pointsOfController;

        for (const auto& event : clipTemplate.events)
        {
            if (event.kind == ClipTemplate::EventKind::cc)
                pointsOfController[event.a].push_back ({ event.tick, event.b });
            else if (event.kind == ClipTemplate::EventKind::programChange)
                clipTemplate.programChanges.push_back ({ event.tick, event.a });
        }

        // A map goes through its keys in order: the controllers by number.
        for (auto& [controller, points] : pointsOfController)
        {
            points.shrink_to_fit();
            clipTemplate.controllerChases.push_back ({ controller, std::move (points) });
        }
    }

} // namespace

Snapshot SnapshotBuilder::build (const model::ProjectModel& project)
{
    const auto tracks = project.getTracks().getTracks();
    const auto referencedClipIds = referencedClipIdsOf (tracks);

    std::vector<ClipTemplate> clipTemplates;
    std::map<juce::String, std::size_t> templateIndexOfClipId;

    for (const auto& clip : project.getClipPool().getClips())
    {
        const auto clipId = clip.getId();

        if (referencedClipIds.count (clipId) == 0)
            continue;

        templateIndexOfClipId[clipId] = clipTemplates.size();
        clipTemplates.push_back (templateOf (clip));
    }

    std::vector<TrackPlan> trackPlans;

    for (const auto& track : tracks)
        trackPlans.push_back (planOf (track, templateIndexOfClipId));

    return Snapshot { TempoMap (tempoSegmentsOf (project.getTimeline())), std::move (clipTemplates), std::move (trackPlans) };
}

ClipTemplate SnapshotBuilder::templateOf (const model::ClipModel& clip)
{
    ClipTemplate clipTemplate;
    clipTemplate.loopLength = clip.getLoopLength();
    jassert (clipTemplate.loopLength > 0);

    if (clip.getKind() == model::ClipModel::Kind::step)
        addContentOfStepClip (clipTemplate, clip.getGrid());
    else
        addContentOfEventClip (clipTemplate, clip);

    // A stable sort: what shares a tick stays in the order it was added in,
    // which is the tie-breaker at equal ticks. For an event clip that is
    // child order, also for notes that quantising has put on one tick; for a
    // step clip it is the order of the lanes, and of the CC values within a
    // cell. The document itself is never sorted.
    std::stable_sort (clipTemplate.notes.begin(), clipTemplate.notes.end(), [] (const ClipTemplate::Note& earlier, const ClipTemplate::Note& later) {
        return earlier.tick < later.tick;
    });
    std::stable_sort (clipTemplate.events.begin(), clipTemplate.events.end(), [] (const ClipTemplate::Event& earlier, const ClipTemplate::Event& later) {
        return earlier.tick < later.tick;
    });

    addChaseTables (clipTemplate);

    // Fixed in size once built: nothing is left over to grow into.
    clipTemplate.controllerChases.shrink_to_fit();
    clipTemplate.programChanges.shrink_to_fit();
    clipTemplate.notes.shrink_to_fit();
    clipTemplate.events.shrink_to_fit();
    clipTemplate.sysexBytes.shrink_to_fit();

    return clipTemplate;
}

std::vector<TempoMap::Segment> SnapshotBuilder::tempoSegmentsOf (const model::TimelineModel& timeline)
{
    std::vector<TempoMap::Segment> segments;

    for (const auto& tempoEvent : timeline.getTempoEvents())
        segments.push_back ({ tempoEvent.getTick(), TempoMap::microsecondsPerQuarterAt (tempoEvent.getBpm()) });

    // The document keeps tempo events in the order they were made; a
    // Timeline has at most one per tick (invariant 6).
    std::sort (segments.begin(), segments.end(), [] (const TempoMap::Segment& earlier, const TempoMap::Segment& later) {
        return earlier.startTick < later.startTick;
    });

    return segments;
}

SnapshotBuilder::Destinations SnapshotBuilder::destinationsWithoutHardware (const model::ProjectModel& project)
{
    Destinations destinations;
    std::map<juce::String, std::size_t> destinationOfPlayerId;

    for (const auto& track : project.getTracks().getTracks())
    {
        const auto playerId = track.getPlayerId();

        if (playerId.isEmpty())
        {
            destinations.destinationOfTrack.push_back (destinations.count);
            ++destinations.count;
            continue;
        }

        const auto known = destinationOfPlayerId.find (playerId);

        if (known != destinationOfPlayerId.end())
        {
            destinations.destinationOfTrack.push_back (known->second);
            continue;
        }

        destinationOfPlayerId[playerId] = destinations.count;
        destinations.destinationOfTrack.push_back (destinations.count);
        ++destinations.count;
    }

    return destinations;
}

} // namespace render
