#include "render/Kernel.h"

#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/TrackPlan.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <optional>

namespace render
{

namespace
{

    Kernel::EventKind kindOf (ClipTemplate::EventKind kind) noexcept
    {
        switch (kind)
        {
            case ClipTemplate::EventKind::cc:
                return Kernel::EventKind::cc;
            case ClipTemplate::EventKind::pitchBend:
                return Kernel::EventKind::pitchBend;
            case ClipTemplate::EventKind::programChange:
                return Kernel::EventKind::programChange;
            case ClipTemplate::EventKind::channelPressure:
                return Kernel::EventKind::channelPressure;
            case ClipTemplate::EventKind::polyPressure:
                return Kernel::EventKind::polyPressure;
            case ClipTemplate::EventKind::sysex:
                return Kernel::EventKind::sysex;
        }

        assert (false);
        return Kernel::EventKind::cc;
    }

// The tick at which a placement ends, which it no longer covers. No end
// lies past the largest tick (docs/SCHEMA.md, invariant 8).
    std::int64_t endOf (const TrackPlan::Placement& placement) noexcept
    {
        return placement.start + placement.length;
    }

// The first placement of a track that has not ended at the tick: the one
// that covers the tick, or the next one after it. The placements of a plan
// are in the order of their starts and do not overlap.
    std::vector<TrackPlan::Placement>::const_iterator firstPlacementEndingAfter (const TrackPlan& plan, std::int64_t tick) noexcept
    {
        return std::partition_point (plan.placements.begin(), plan.placements.end(), [tick] (const TrackPlan::Placement& placement) {
            return endOf (placement) <= tick;
        });
    }

// Where in the clip's loop a placement is at a tick that it covers: it
// begins `offset` ticks into the loop and goes round from there.
//
// Worked out without a sign: the offset and the distance from the
// placement's start may each be nearly as large as the largest tick, and
// their sum then fits only so.
    std::int64_t phaseAt (const TrackPlan::Placement& placement, std::int64_t loopLength, std::int64_t tick) noexcept
    {
        const auto ticksIntoPlacement = static_cast<std::uint64_t> (tick - placement.start);
        const auto ticksIntoClip = static_cast<std::uint64_t> (placement.offset) + ticksIntoPlacement;

        return static_cast<std::int64_t> (ticksIntoClip % static_cast<std::uint64_t> (loopLength));
    }

// Whether a placement goes on where the one before it on its track ends: it
// follows at once, has the same clip, and begins at the tick of the loop to
// which the earlier one has come by its end. Two such placements are played
// as one (docs/RENDERING.md -> Per track): that is what a split leaves.
    bool continuesWithoutABreak (const TrackPlan::Placement& earlier, const TrackPlan::Placement& later, std::int64_t loopLength) noexcept
    {
        return later.templateIndex == earlier.templateIndex
            && later.start == endOf (earlier)
            && later.offset == phaseAt (earlier, loopLength, endOf (earlier));
    }

// How many ticks a note that begins at a tick in a placement may sound: its
// length, or less when its placement ends first. A placement that is
// continued without a break does not end for the note; the cut comes at the
// end of the last placement that is one with this one.
//
// Only the placements the note reaches into are looked at, so a track cut
// into many pieces costs a note no more than the pieces it spans.
    std::int64_t lengthWithinMergedPlacements (const TrackPlan& plan,
                                               std::vector<TrackPlan::Placement>::const_iterator placement,
                                               std::int64_t loopLength,
                                               std::int64_t tick,
                                               std::int64_t noteLength) noexcept
    {
        auto last = placement;

        for (auto next = std::next (last); next != plan.placements.end(); ++next)
        {
            if (endOf (*last) - tick >= noteLength || ! continuesWithoutABreak (*last, *next, loopLength))
                break;

            last = next;
        }

        // The tick lies inside the first of them, so at least one tick is left.
        return std::min (noteLength, endOf (*last) - tick);
    }

// How many ticks a loop that stands at `phase` has to go until the next of
// these notes or events comes: 0 when one stands at the phase itself, and
// through the loop point when none stands at or after it. Nothing when the
// list is empty. Always less than the loop length.
    template <typename NoteOrEvent>
    std::optional<std::int64_t> ticksUntilNext (const std::vector<NoteOrEvent>& inTickOrder, std::int64_t phase, std::int64_t loopLength) noexcept
    {
        if (inTickOrder.empty())
            return std::nullopt;

        const auto next = std::lower_bound (inTickOrder.begin(), inTickOrder.end(), phase, [] (const NoteOrEvent& noteOrEvent, std::int64_t tick) {
            return noteOrEvent.tick < tick;
        });

        if (next != inTickOrder.end())
            return next->tick - phase;

        return (loopLength - phase) + inTickOrder.front().tick;
    }

// The first tick from `from` on at which a track plays a note or an event,
// or `end` when it plays nothing before that.
    std::int64_t nextTickWithContent (const Snapshot& snapshot, const TrackPlan& plan, std::int64_t from, std::int64_t end) noexcept
    {
        for (auto placement = firstPlacementEndingAfter (plan, from); placement != plan.placements.end() && placement->start < end; ++placement)
        {
            const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];

            const std::int64_t searchStart = std::max (from, placement->start);
            const std::int64_t phase = phaseAt (*placement, clipTemplate.loopLength, searchStart);

            const auto untilEvent = ticksUntilNext (clipTemplate.events, phase, clipTemplate.loopLength);
            const auto untilNote = ticksUntilNext (clipTemplate.notes, phase, clipTemplate.loopLength);

            if (! untilEvent.has_value() && ! untilNote.has_value())
                continue;

            const std::int64_t untilContent = std::min (untilEvent.value_or (clipTemplate.loopLength), untilNote.value_or (clipTemplate.loopLength));

            // The placement ends before its clip comes to that content.
            if (untilContent >= endOf (*placement) - searchStart)
                continue;

            // A later placement has only later ticks.
            return std::min (searchStart + untilContent, end);
        }

        return end;
    }

// The caller's buffer while it is filled, and the count of the events at the
// tick in hand, by which a later call finds its place again.
    struct Output
    {
        std::span<Kernel::Event> buffer;
        std::size_t eventCount = 0;

        // Of the events at the tick in hand, how many an earlier call has
        // given already, and how many have come by in this call.
        std::size_t eventsToSkipAtTick = 0;
        std::size_t eventsSeenAtTick = 0;

        void beginTick (std::size_t eventsAlreadyGiven) noexcept
        {
            eventsToSkipAtTick = eventsAlreadyGiven;
            eventsSeenAtTick = 0;
        }

        // Takes the next event of the tick in hand. False when the buffer has
        // no room for it: the event is then not counted as seen.
        bool take (const Kernel::Event& event) noexcept
        {
            if (eventsSeenAtTick >= eventsToSkipAtTick)
            {
                if (eventCount == buffer.size())
                    return false;

                buffer[eventCount] = event;
                ++eventCount;
            }

            ++eventsSeenAtTick;
            return true;
        }
    };

// Everything one track plays at a tick: its events that are no notes, then
// its notes. False when the buffer ran full on the way.
    bool playTrackAt (const Snapshot& snapshot, std::size_t trackIndex, std::int64_t tick, Output& output) noexcept
    {
        const TrackPlan& plan = snapshot.trackPlans[trackIndex];
        const auto placement = firstPlacementEndingAfter (plan, tick);

        if (placement == plan.placements.end() || placement->start > tick)
            return true;

        const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];
        const std::int64_t phase = phaseAt (*placement, clipTemplate.loopLength, tick);

        const auto firstEvent = std::lower_bound (clipTemplate.events.begin(), clipTemplate.events.end(), phase, [] (const ClipTemplate::Event& event, std::int64_t eventTick) {
            return event.tick < eventTick;
        });

        for (auto event = firstEvent; event != clipTemplate.events.end() && event->tick == phase; ++event)
        {
            Kernel::Event played;
            played.tick = tick;
            played.track = trackIndex;
            played.channel = plan.channel;
            played.kind = kindOf (event->kind);
            played.a = event->a;
            played.b = event->b;

            if (event->kind == ClipTemplate::EventKind::sysex)
            {
                played.templateIndex = placement->templateIndex;
                played.dataStart = event->dataStart;
                played.dataLength = event->dataLength;
            }

            if (! output.take (played))
                return false;
        }

        const auto firstNote = std::lower_bound (clipTemplate.notes.begin(), clipTemplate.notes.end(), phase, [] (const ClipTemplate::Note& note, std::int64_t noteTick) {
            return note.tick < noteTick;
        });

        for (auto note = firstNote; note != clipTemplate.notes.end() && note->tick == phase; ++note)
        {
            Kernel::Event played;
            played.tick = tick;
            played.track = trackIndex;
            played.channel = plan.channel;
            played.kind = Kernel::EventKind::noteOn;
            played.a = note->pitch;
            played.b = note->velocity;
            played.offTick = tick + lengthWithinMergedPlacements (plan, placement, clipTemplate.loopLength, tick, note->length);
            played.offVelocity = note->offVelocity;

            if (! output.take (played))
                return false;
        }

        return true;
    }

// The last value of a chase table that a placement has played before a tick,
// or nothing when it has played none of them by then. The tick lies behind
// the placement's start; when it lies behind its end too, the placement has
// played all it ever plays.
//
// Counted without a sign, as in phaseAt.
    std::optional<int> lastValuePlayedBefore (const std::vector<ClipTemplate::ChasePoint>& points,
                                              const TrackPlan::Placement& placement,
                                              std::int64_t loopLength,
                                              std::int64_t tick) noexcept
    {
        if (points.empty())
            return std::nullopt;

        const auto ticksPlayed = static_cast<std::uint64_t> (std::min (tick, endOf (placement)) - placement.start);
        const auto ticksIntoClip = static_cast<std::uint64_t> (placement.offset) + ticksPlayed;

        // How often the placement has passed the loop point, and where in
        // the loop it stands.
        const std::uint64_t passesBefore = ticksIntoClip / static_cast<std::uint64_t> (loopLength);
        const auto phase = static_cast<std::int64_t> (ticksIntoClip % static_cast<std::uint64_t> (loopLength));

        // The first pass began at the offset, and what stands before it was
        // not played then.
        const auto wasPlayedInPass = [&placement] (const ClipTemplate::ChasePoint& point, std::uint64_t pass) {
            return pass > 0 || point.tick >= placement.offset;
        };

        // In the pass in hand: the last point before the phase.
        const auto firstPointNotBefore = std::lower_bound (points.begin(), points.end(), phase, [] (const ClipTemplate::ChasePoint& point, std::int64_t pointTick) {
            return point.tick < pointTick;
        });

        if (firstPointNotBefore != points.begin())
        {
            const ClipTemplate::ChasePoint& lastBefore = *std::prev (firstPointNotBefore);

            if (wasPlayedInPass (lastBefore, passesBefore))
                return lastBefore.value;

            // It stands before the offset, and so does every point before
            // it: the first pass has played none so far, and there is no
            // pass before the first.
            return std::nullopt;
        }

        // The pass before went to the end of the loop, so its last point is
        // the table's last.
        if (passesBefore > 0 && wasPlayedInPass (points.back(), passesBefore - 1))
            return points.back().value;

        return std::nullopt;
    }

} // namespace

Kernel::ChasedValues Kernel::chase (const Snapshot& snapshot, std::size_t trackIndex, std::int64_t tick) noexcept
{
    assert (trackIndex < snapshot.trackPlans.size());
    assert (tick >= 0);

    const TrackPlan& plan = snapshot.trackPlans[trackIndex];
    ChasedValues values;

    // The placements that begin before the tick, the latest first: only
    // they have played anything by then.
    const auto firstNotBegun = std::partition_point (plan.placements.begin(), plan.placements.end(), [tick] (const TrackPlan::Placement& placement) {
        return placement.start < tick;
    });

    for (auto placement = firstNotBegun; placement != plan.placements.begin();)
    {
        --placement;

        const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];

        // The first value found for a controller is its last before the
        // tick; an earlier placement no longer changes it.
        for (const ClipTemplate::ControllerChase& controllerChase : clipTemplate.controllerChases)
        {
            int& value = values.controllerValues[static_cast<std::size_t> (controllerChase.controller)];

            if (value == ChasedValues::none)
                value = lastValuePlayedBefore (controllerChase.points, *placement, clipTemplate.loopLength, tick).value_or (ChasedValues::none);
        }

        if (values.program == ChasedValues::none)
            values.program = lastValuePlayedBefore (clipTemplate.programChanges, *placement, clipTemplate.loopLength, tick).value_or (ChasedValues::none);
    }

    return values;
}

Kernel::Progress Kernel::expand (const Snapshot& snapshot,
                                 const std::vector<bool>& audibleTracks,
                                 Position from,
                                 std::int64_t end,
                                 std::span<Event> buffer) noexcept
{
    assert (audibleTracks.size() == snapshot.trackPlans.size());
    assert (from.tick >= 0 && from.tick <= end);
    assert (! buffer.empty());

    Output output;
    output.buffer = buffer;

    std::int64_t searchFrom = from.tick;

    while (searchFrom < end)
    {
        // The next tick at which any track that is heard plays something.
        std::int64_t tick = end;

        for (std::size_t trackIndex = 0; trackIndex < snapshot.trackPlans.size(); ++trackIndex)
            if (audibleTracks[trackIndex])
                tick = nextTickWithContent (snapshot, snapshot.trackPlans[trackIndex], searchFrom, tick);

        if (tick == end)
            break;

        // Only the tick the caller comes back to can have events that are
        // given already.
        output.beginTick (tick == from.tick ? from.eventsTaken : 0);

        for (std::size_t trackIndex = 0; trackIndex < snapshot.trackPlans.size(); ++trackIndex)
            if (audibleTracks[trackIndex] && ! playTrackAt (snapshot, trackIndex, tick, output))
                return { output.eventCount, { tick, output.eventsSeenAtTick } };

        // tick < end, so there is a next one.
        searchFrom = tick + 1;
    }

    return { output.eventCount, { end, 0 } };
}

} // namespace render
