#include "render/Kernel.h"

#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/TrackPlan.h"

#include <algorithm>
#include <bitset>
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

// The placements of a track itself, or of one of its AutomationLanes: in
// the order of their starts, none sharing a tick with another.
    using Placements = std::vector<TrackPlan::Placement>;

// A set of controllers, by number.
    using Controllers = std::bitset<128>;

// The first of the placements that has not ended at the tick: the one that
// covers the tick, or the next one after it.
    Placements::const_iterator firstPlacementEndingAfter (const Placements& placements, std::int64_t tick) noexcept
    {
        return std::partition_point (placements.begin(), placements.end(), [tick] (const TrackPlan::Placement& placement) {
            return endOf (placement) <= tick;
        });
    }

// The one of the placements that covers the tick, or their end when none
// does.
    Placements::const_iterator placementCovering (const Placements& placements, std::int64_t tick) noexcept
    {
        const auto placement = firstPlacementEndingAfter (placements, tick);

        return placement != placements.end() && placement->start <= tick ? placement : placements.end();
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
    std::int64_t lengthWithinMergedPlacements (const Placements& placements,
                                               Placements::const_iterator placement,
                                               std::int64_t loopLength,
                                               std::int64_t tick,
                                               std::int64_t noteLength) noexcept
    {
        auto last = placement;

        for (auto next = std::next (last); next != placements.end(); ++next)
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

// The first tick from `from` on at which one of the placements plays a note
// or an event of its clip, or `end` when none does before that.
    std::int64_t nextTickWithContentIn (const Snapshot& snapshot, const Placements& placements, std::int64_t from, std::int64_t end) noexcept
    {
        for (auto placement = firstPlacementEndingAfter (placements, from); placement != placements.end() && placement->start < end; ++placement)
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

// The first tick from `from` on at which one of the placements ends, or
// `end` when none does before that.
    std::int64_t nextEndIn (const Placements& placements, std::int64_t from, std::int64_t end) noexcept
    {
        const auto placement = std::partition_point (placements.begin(), placements.end(), [from] (const TrackPlan::Placement& candidate) {
            return endOf (candidate) < from;
        });

        return placement != placements.end() ? std::min (endOf (*placement), end) : end;
    }

// The first tick from `from` on at which a track may have something to play,
// or `end` when it has nothing before that: a note or an event of one of its
// own clips, an event of one of its AutomationLanes, or the end of a lane's
// placement, where a value may be restored. A tick found here may still give
// nothing, when a lane owns what stands there or no value is to be restored.
    std::int64_t nextTickWithContent (const Snapshot& snapshot, const TrackPlan& plan, std::int64_t from, std::int64_t end) noexcept
    {
        std::int64_t tick = nextTickWithContentIn (snapshot, plan.placements, from, end);

        for (const TrackPlan::AutomationLane& lane : plan.automationLanes)
        {
            tick = nextTickWithContentIn (snapshot, lane.placements, from, tick);
            tick = nextEndIn (lane.placements, from, tick);
        }

        return tick;
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

// The chase table of a controller in a template, or nothing when the clip
// never sends it. The tables are in the order of the controller numbers.
    const std::vector<ClipTemplate::ChasePoint>* chasePointsOf (const ClipTemplate& clipTemplate, int controller) noexcept
    {
        const auto table = std::lower_bound (clipTemplate.controllerChases.begin(), clipTemplate.controllerChases.end(), controller, [] (const ClipTemplate::ControllerChase& controllerChase, int number) {
            return controllerChase.controller < number;
        });

        if (table == clipTemplate.controllerChases.end() || table->controller != controller)
            return nullptr;

        return &table->points;
    }

// What the track's own clips have last sent of a controller before a tick,
// whether or not a lane owned the controller then: the track's own chase
// (docs/RENDERING.md -> Chase), for one controller. Its placements are gone
// through backwards from the tick, and the first value found is the last
// one before it.
    std::optional<int> valueOfTrackBefore (const Snapshot& snapshot, const TrackPlan& plan, int controller, std::int64_t tick) noexcept
    {
        const auto firstNotBegun = std::partition_point (plan.placements.begin(), plan.placements.end(), [tick] (const TrackPlan::Placement& placement) {
            return placement.start < tick;
        });

        for (auto placement = firstNotBegun; placement != plan.placements.begin();)
        {
            --placement;

            const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];
            const auto* points = chasePointsOf (clipTemplate, controller);

            if (points == nullptr)
                continue;

            const auto value = lastValuePlayedBefore (*points, *placement, clipTemplate.loopLength, tick);

            if (value.has_value())
                return value;
        }

        return std::nullopt;
    }

// Every controller that a lane from `firstLane` on owns at a tick: what the
// clips of their placements that cover the tick send (docs/RENDERING.md ->
// AutomationLane merge).
    Controllers controllersOwnedAt (const Snapshot& snapshot, const TrackPlan& plan, std::size_t firstLane, std::int64_t tick) noexcept
    {
        Controllers owned;

        for (std::size_t laneIndex = firstLane; laneIndex < plan.automationLanes.size(); ++laneIndex)
        {
            const Placements& placements = plan.automationLanes[laneIndex].placements;
            const auto placement = placementCovering (placements, tick);

            if (placement != placements.end())
                owned |= snapshot.clipTemplates[placement->templateIndex].controllers;
        }

        return owned;
    }

// A placement of one of a track's AutomationLanes.
    struct LanePlacement
    {
        std::size_t laneIndex = 0;
        Placements::const_iterator placement;
    };

// The lane placement that has a controller at a tick: of the placements that
// cover the tick and own the controller, the one on the latest lane. Nothing
// when no lane owns it there; the track's own clips then have it.
    std::optional<LanePlacement> lanePlacementInChargeAt (const Snapshot& snapshot, const TrackPlan& plan, int controller, std::int64_t tick) noexcept
    {
        for (std::size_t laneIndex = plan.automationLanes.size(); laneIndex > 0;)
        {
            --laneIndex;

            const Placements& placements = plan.automationLanes[laneIndex].placements;
            const auto placement = placementCovering (placements, tick);

            if (placement != placements.end() && snapshot.clipTemplates[placement->templateIndex].controllers.test (static_cast<std::size_t> (controller)))
                return LanePlacement { laneIndex, placement };
        }

        return std::nullopt;
    }

// What a lane placement's clip has last sent of a controller before a tick,
// counted from where the placement began, whether or not a later lane had
// the controller then. Placements that continue each other without a break
// are one: where this one has sent nothing yet, the one it continues is
// asked. The placement owns the controller, and the tick is not before its
// start.
    std::optional<int> valueOfLanePlacementBefore (const Snapshot& snapshot,
                                                   const Placements& placements,
                                                   Placements::const_iterator placement,
                                                   int controller,
                                                   std::int64_t tick) noexcept
    {
        const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];
        const auto* points = chasePointsOf (clipTemplate, controller);
        assert (points != nullptr);

        if (points == nullptr)
            return std::nullopt;

        for (auto piece = placement;; --piece)
        {
            const auto value = lastValuePlayedBefore (*points, *piece, clipTemplate.loopLength, tick);

            if (value.has_value())
                return value;

            if (piece == placements.begin() || ! continuesWithoutABreak (*std::prev (piece), *piece, clipTemplate.loopLength))
                return std::nullopt;
        }
    }

// Of the lane placements that own a controller and end at `limit` or before
// it, on the lanes from `firstLane` on: the latest end. Nothing when there
// is no such placement.
    std::optional<std::int64_t> latestEndOfOwningPlacement (const Snapshot& snapshot,
                                                            const TrackPlan& plan,
                                                            std::size_t firstLane,
                                                            int controller,
                                                            std::int64_t limit) noexcept
    {
        std::optional<std::int64_t> latestEnd;

        for (std::size_t laneIndex = firstLane; laneIndex < plan.automationLanes.size(); ++laneIndex)
        {
            const Placements& placements = plan.automationLanes[laneIndex].placements;

            const auto firstEndingLater = std::partition_point (placements.begin(), placements.end(), [limit] (const TrackPlan::Placement& placement) {
                return endOf (placement) <= limit;
            });

            // The lane's ends are in order, so its first owning placement
            // from behind has its latest.
            for (auto placement = firstEndingLater; placement != placements.begin();)
            {
                --placement;

                if (snapshot.clipTemplates[placement->templateIndex].controllers.test (static_cast<std::size_t> (controller)))
                {
                    latestEnd = std::max (latestEnd.value_or (0), endOf (*placement));
                    break;
                }
            }
        }

        return latestEnd;
    }

// The last value of a controller that a track with AutomationLanes has sent
// before a tick, played from tick 0: what a seek to the tick sends
// (docs/RENDERING.md -> Chase; decided 25 Nov 2026: a seek leaves a
// controller as playing through would have).
//
// Who had the controller just before the tick is asked first:
//
// - a lane placement: its own last value, if it has sent one. If not, the
//   synth still holds what it held when the placement took the controller
//   over: at its start, or where a later lane's placement, which had the
//   controller until then, ended. Nothing is sent at such a tick, so the
//   question is asked again for it;
// - the track's own clips: their last value, if they have one. It went out
//   either at its own tick or, when a lane owned the controller then, where
//   that lane's placement ended. If the clips have none, the synth holds
//   what the last lane placement that owned the controller left behind, and
//   the question is asked again for the tick at which that one ended.
//
// Each time round the tick lies earlier, and each round but the last passes
// a lane placement, so their number bounds the rounds. Allocates nothing.
    std::optional<int> lastValueSentBefore (const Snapshot& snapshot, const TrackPlan& plan, int controller, std::int64_t tick) noexcept
    {
        std::int64_t before = tick;

        while (before > 0)
        {
            const auto inCharge = lanePlacementInChargeAt (snapshot, plan, controller, before - 1);

            if (! inCharge.has_value())
            {
                const auto valueOfTrack = valueOfTrackBefore (snapshot, plan, controller, before);

                if (valueOfTrack.has_value())
                    return valueOfTrack;

                const auto latestEnd = latestEndOfOwningPlacement (snapshot, plan, 0, controller, before - 1);

                if (! latestEnd.has_value())
                    return std::nullopt;

                before = *latestEnd;
                continue;
            }

            const Placements& placements = plan.automationLanes[inCharge->laneIndex].placements;
            const auto valueOfLane = valueOfLanePlacementBefore (snapshot, placements, inCharge->placement, controller, before);

            if (valueOfLane.has_value())
                return valueOfLane;

            const auto endOfLaterLane = latestEndOfOwningPlacement (snapshot, plan, inCharge->laneIndex + 1, controller, before - 1);

            before = std::max (inCharge->placement->start, endOfLaterLane.value_or (0));
        }

        return std::nullopt;
    }

// The controllers whose lane placement ends at a tick: those that a
// placement ending there owned and had, no later lane owning them just
// before the tick. A placement that is continued without a break does not
// end. Each of these controllers passes to whoever has it from the tick on,
// and that one's value is restored (docs/RENDERING.md -> AutomationLane
// merge).
    Controllers controllersGivenUpAt (const Snapshot& snapshot, const TrackPlan& plan, std::int64_t tick) noexcept
    {
        Controllers givenUp;

        if (tick == 0)
            return givenUp;

        for (std::size_t laneIndex = 0; laneIndex < plan.automationLanes.size(); ++laneIndex)
        {
            const Placements& placements = plan.automationLanes[laneIndex].placements;
            const auto placement = placementCovering (placements, tick - 1);

            if (placement == placements.end() || endOf (*placement) != tick)
                continue;

            const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];
            const auto next = std::next (placement);

            if (next != placements.end() && continuesWithoutABreak (*placement, *next, clipTemplate.loopLength))
                continue;

            givenUp |= clipTemplate.controllers & ~controllersOwnedAt (snapshot, plan, laneIndex + 1, tick - 1);
        }

        return givenUp;
    }

// The value that goes out for a controller at the tick at which the lane
// placement that had it ends: the last value of whoever has it from that
// tick on. That is an earlier lane's placement that still runs (decided 25
// Nov 2026), with what its clip has last sent while the later lane had the
// controller, or the track's own clips, with what they have last sent before
// the tick, before the lane placement or under it (decided 25 Nov 2026).
// Nothing when that one has sent no value yet: the synth keeps the lane's.
    std::optional<int> valueRestoredAt (const Snapshot& snapshot, const TrackPlan& plan, int controller, std::int64_t tick) noexcept
    {
        const auto inCharge = lanePlacementInChargeAt (snapshot, plan, controller, tick);

        if (inCharge.has_value())
            return valueOfLanePlacementBefore (snapshot, plan.automationLanes[inCharge->laneIndex].placements, inCharge->placement, controller, tick);

        return valueOfTrackBefore (snapshot, plan, controller, tick);
    }

// What every event of one track at one tick has in common.
    Kernel::Event eventOfTrackAt (const TrackPlan& plan, std::size_t trackIndex, std::int64_t tick) noexcept
    {
        Kernel::Event event;
        event.tick = tick;
        event.track = trackIndex;
        event.channel = plan.channel;
        return event;
    }

// The values restored at a tick, by ascending controller number. False when
// the buffer ran full on the way.
    bool playRestoredValuesAt (const Snapshot& snapshot, std::size_t trackIndex, std::int64_t tick, Output& output) noexcept
    {
        const TrackPlan& plan = snapshot.trackPlans[trackIndex];
        const Controllers givenUp = controllersGivenUpAt (snapshot, plan, tick);

        if (givenUp.none())
            return true;

        for (std::size_t controller = 0; controller < givenUp.size(); ++controller)
        {
            if (! givenUp.test (controller))
                continue;

            const auto value = valueRestoredAt (snapshot, plan, static_cast<int> (controller), tick);

            if (! value.has_value())
                continue;

            Kernel::Event played = eventOfTrackAt (plan, trackIndex, tick);
            played.kind = Kernel::EventKind::cc;
            played.a = static_cast<int> (controller);
            played.b = *value;

            if (! output.take (played))
                return false;
        }

        return true;
    }

// The events that a placement's clip has at a tick the placement covers, in
// the order of the template, without the cc events of the controllers that
// are held back. False when the buffer ran full on the way.
    bool playEventsOfPlacementAt (const Snapshot& snapshot,
                                  std::size_t trackIndex,
                                  const TrackPlan::Placement& placement,
                                  const Controllers& heldBack,
                                  std::int64_t tick,
                                  Output& output) noexcept
    {
        const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement.templateIndex];
        const std::int64_t phase = phaseAt (placement, clipTemplate.loopLength, tick);

        const auto firstEvent = std::lower_bound (clipTemplate.events.begin(), clipTemplate.events.end(), phase, [] (const ClipTemplate::Event& event, std::int64_t eventTick) {
            return event.tick < eventTick;
        });

        for (auto event = firstEvent; event != clipTemplate.events.end() && event->tick == phase; ++event)
        {
            if (event->kind == ClipTemplate::EventKind::cc && heldBack.test (static_cast<std::size_t> (event->a)))
                continue;

            Kernel::Event played = eventOfTrackAt (snapshot.trackPlans[trackIndex], trackIndex, tick);
            played.kind = kindOf (event->kind);
            played.a = event->a;
            played.b = event->b;

            if (event->kind == ClipTemplate::EventKind::sysex)
            {
                played.templateIndex = placement.templateIndex;
                played.dataStart = event->dataStart;
                played.dataLength = event->dataLength;
            }

            if (! output.take (played))
                return false;
        }

        return true;
    }

// Everything one track plays at a tick (docs/RENDERING.md -> Equal-tick
// order, decided 25 Nov 2026 for what the lanes add): the values restored
// where a lane placement ends, then the events of the track's own clip,
// then those of each lane in the order of the lanes, then the track's
// notes. A cc event is held back when a lane owns its controller: one of the
// track's clip by any lane, one of a lane by a later lane. False when the
// buffer ran full on the way.
    bool playTrackAt (const Snapshot& snapshot, std::size_t trackIndex, std::int64_t tick, Output& output) noexcept
    {
        const TrackPlan& plan = snapshot.trackPlans[trackIndex];

        if (! playRestoredValuesAt (snapshot, trackIndex, tick, output))
            return false;

        const auto placement = placementCovering (plan.placements, tick);

        if (placement != plan.placements.end()
            && ! playEventsOfPlacementAt (snapshot, trackIndex, *placement, controllersOwnedAt (snapshot, plan, 0, tick), tick, output))
            return false;

        for (std::size_t laneIndex = 0; laneIndex < plan.automationLanes.size(); ++laneIndex)
        {
            const Placements& placements = plan.automationLanes[laneIndex].placements;
            const auto lanePlacement = placementCovering (placements, tick);

            if (lanePlacement != placements.end()
                && ! playEventsOfPlacementAt (snapshot, trackIndex, *lanePlacement, controllersOwnedAt (snapshot, plan, laneIndex + 1, tick), tick, output))
                return false;
        }

        if (placement == plan.placements.end())
            return true;

        const ClipTemplate& clipTemplate = snapshot.clipTemplates[placement->templateIndex];
        const std::int64_t phase = phaseAt (*placement, clipTemplate.loopLength, tick);

        const auto firstNote = std::lower_bound (clipTemplate.notes.begin(), clipTemplate.notes.end(), phase, [] (const ClipTemplate::Note& note, std::int64_t noteTick) {
            return note.tick < noteTick;
        });

        for (auto note = firstNote; note != clipTemplate.notes.end() && note->tick == phase; ++note)
        {
            Kernel::Event played = eventOfTrackAt (plan, trackIndex, tick);
            played.kind = Kernel::EventKind::noteOn;
            played.a = note->pitch;
            played.b = note->velocity;
            played.offTick = tick + lengthWithinMergedPlacements (plan.placements, placement, clipTemplate.loopLength, tick, note->length);
            played.offVelocity = note->offVelocity;

            if (! output.take (played))
                return false;
        }

        return true;
    }

} // namespace

Kernel::ChasedValues Kernel::chase (const Snapshot& snapshot, std::size_t trackIndex, std::int64_t tick) noexcept
{
    assert (trackIndex < snapshot.trackPlans.size());
    assert (tick >= 0);

    const TrackPlan& plan = snapshot.trackPlans[trackIndex];
    ChasedValues values;

    // First the track's own clips. The placements that begin before the
    // tick, the latest first: only they have played anything by then.
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

    // Then the controllers that a lane has had before the tick: for them the
    // clips' own last value need not be the last one sent.
    Controllers ownedBefore;

    for (const TrackPlan::AutomationLane& lane : plan.automationLanes)
        for (const TrackPlan::Placement& placement : lane.placements)
            if (placement.start < tick)
                ownedBefore |= snapshot.clipTemplates[placement.templateIndex].controllers;

    for (std::size_t controller = 0; controller < ownedBefore.size(); ++controller)
        if (ownedBefore.test (controller))
            values.controllerValues[controller] = lastValueSentBefore (snapshot, plan, static_cast<int> (controller), tick).value_or (ChasedValues::none);

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
