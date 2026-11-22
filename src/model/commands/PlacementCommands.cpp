#include "model/commands/PlacementCommands.h"

#include "model/AutomationClipRule.h"
#include "model/ClipModel.h"
#include "model/Ids.h"
#include "model/PlacementModel.h"
#include "model/commands/NodeLookup.h"

#include <limits>

namespace model::commands
{

namespace
{

    const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

    juce::ValueTree trackOf (const juce::ValueTree& project, const juce::String& trackId)
    {
        jassert (project.hasType (ids::Project));
        return NodeLookup::childWithId (project.getChildWithName (ids::Tracks), ids::Track, trackId);
    }

    juce::ValueTree clipOf (const juce::ValueTree& project, const juce::String& clipId)
    {
        return NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId);
    }

    // The automation lane with this id, on whichever track it is.
    juce::ValueTree laneOf (const juce::ValueTree& project, const juce::String& laneId)
    {
        jassert (project.hasType (ids::Project));

        for (const auto& track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
        {
            const auto lane = NodeLookup::childWithId (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane, laneId);

            if (lane.isValid())
                return lane;
        }

        return {};
    }

    CommandResult noSuchClip (const juce::String& clipId)
    {
        return CommandResult::refused ("There is no Clip with the id \"" + clipId + "\".");
    }

    CommandResult noSuchPlacement (const juce::String& placementId)
    {
        return CommandResult::refused ("There is no Placement with the id \"" + placementId + "\".");
    }

    CommandResult noSuchTrack (const juce::String& trackId)
    {
        return CommandResult::refused ("There is no Track with the id \"" + trackId + "\".");
    }

    // Why a placement cannot have this start and this length; empty when it
    // can.
    juce::String whyNoExtent (std::int64_t start, std::int64_t length)
    {
        if (start < 0)
            return "A placement starts at tick 0 or later, not at " + juce::String (start) + ".";

        if (length < 1)
            return "A placement is at least one tick long, not " + juce::String (length) + ".";

        if (length > largestTick - start)
            return "A placement that starts at tick " + juce::String (start) + " cannot be " + juce::String (length) + " ticks long: it would end past the largest tick there is.";

        return {};
    }

    // The first tick after a placement of the document. The Validator lets
    // a placement of a loaded file end past the largest tick; such an end is
    // held at the largest tick, as the Validator holds it, so that the sum
    // cannot overflow.
    std::int64_t endOf (const PlacementModel& placement)
    {
        const auto start = placement.getStart();
        const auto length = placement.getLength();

        return length > largestTick - start ? largestTick : start + length;
    }

    // The placement of this parent — a track's Placements or an
    // AutomationLane — that shares a tick with [start, start + length);
    // invalid when there is none. `moving` is the placement that is to get
    // this extent and so is not in its own way; invalid for a new one.
    juce::ValueTree placementInTheWay (const juce::ValueTree& parent, std::int64_t start, std::int64_t length, const juce::ValueTree& moving)
    {
        const auto end = start + length;

        for (const auto& sibling : NodeLookup::childrenOfType (parent, ids::Placement))
        {
            if (sibling == moving)
                continue;

            const PlacementModel other (sibling);

            if (start < endOf (other) && other.getStart() < end)
                return sibling;
        }

        return {};
    }

    CommandResult inTheWay (const juce::ValueTree& sibling)
    {
        const PlacementModel other (sibling);

        return CommandResult::refused ("Another placement is in the way: it covers the ticks from " + juce::String (other.getStart()) + " up to " + juce::String (endOf (other)) + ".");
    }

    // What both a start and a length must pass before a placement gets
    // them: a possible extent, and nothing in the way under `parent`.
    // Applied when it passes.
    CommandResult checkExtent (const juce::ValueTree& parent, std::int64_t start, std::int64_t length, const juce::ValueTree& moving)
    {
        const auto reason = whyNoExtent (start, length);

        if (reason.isNotEmpty())
            return CommandResult::refused (reason);

        const auto sibling = placementInTheWay (parent, start, length, moving);

        if (sibling.isValid())
            return inTheWay (sibling);

        return CommandResult::applied();
    }

    // Makes a placement of the clip with a new id and puts it behind the
    // others under `parent`, a track's Placements or an AutomationLane.
    // Everything has been checked by then. The result carries the id.
    CommandResult appendNewPlacement (juce::ValueTree parent, const juce::String& clipId, std::int64_t start, std::int64_t length, juce::UndoManager& undoManager)
    {
        const auto id = juce::Uuid().toString();

        // Built apart and added whole, so that undo knows one action. No
        // offset is stored: a new placement starts the loop at its
        // beginning.
        juce::ValueTree placement (ids::Placement);
        placement.setProperty (ids::id, id, nullptr);
        placement.setProperty (ids::clipId, clipId, nullptr);
        placement.setProperty (ids::start, static_cast<juce::int64> (start), nullptr);
        placement.setProperty (ids::length, static_cast<juce::int64> (length), nullptr);

        parent.appendChild (placement, &undoManager);

        return CommandResult::appliedAndCreated (id);
    }

    // (offset + elapsed) mod loopLength, for 0 <= offset < loopLength and
    // elapsed >= 0. Both are brought below the loop length first and added
    // as unsigned numbers: a loop may be as long as an int64 allows, and the
    // plain sum could then pass the largest int64.
    std::int64_t phaseAfter (std::int64_t offset, std::int64_t elapsed, std::int64_t loopLength)
    {
        jassert (loopLength > 0 && offset >= 0 && elapsed >= 0);

        const auto loop = static_cast<std::uint64_t> (loopLength);
        const auto phase = (static_cast<std::uint64_t> (offset) % loop + static_cast<std::uint64_t> (elapsed) % loop) % loop;

        return static_cast<std::int64_t> (phase);
    }

} // namespace

CommandResult addPlacement (juce::ValueTree project, const juce::String& trackId, const juce::String& clipId, std::int64_t start, std::int64_t length, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    if (! clipOf (project, clipId).isValid())
        return noSuchClip (clipId);

    auto placements = track.getChildWithName (ids::Placements);

    const auto extentCheck = checkExtent (placements, start, length, {});

    if (! extentCheck.wasApplied())
        return extentCheck;

    return appendNewPlacement (placements, clipId, start, length, undoManager);
}

CommandResult addLanePlacement (juce::ValueTree project, const juce::String& laneId, const juce::String& clipId, std::int64_t start, std::int64_t length, juce::UndoManager& undoManager)
{
    auto lane = laneOf (project, laneId);

    if (! lane.isValid())
        return CommandResult::refused ("There is no AutomationLane with the id \"" + laneId + "\".");

    const auto clip = clipOf (project, clipId);

    if (! clip.isValid())
        return noSuchClip (clipId);

    const auto whyNot = AutomationClipRule::whyNotForALane (clip);

    if (whyNot.isNotEmpty())
        return CommandResult::refused (whyNot);

    const auto extentCheck = checkExtent (lane, start, length, {});

    if (! extentCheck.wasApplied())
        return extentCheck;

    return appendNewPlacement (lane, clipId, start, length, undoManager);
}

CommandResult movePlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t toStart, juce::UndoManager& undoManager)
{
    auto placement = NodeLookup::placementWithId (project, placementId);

    if (! placement.isValid())
        return noSuchPlacement (placementId);

    const auto extentCheck = checkExtent (placement.getParent(), toStart, PlacementModel (placement).getLength(), placement);

    if (! extentCheck.wasApplied())
        return extentCheck;

    placement.setProperty (ids::start, static_cast<juce::int64> (toStart), &undoManager);

    return CommandResult::applied();
}

CommandResult movePlacementToTrack (juce::ValueTree project, const juce::String& placementId, const juce::String& toTrackId, std::int64_t toStart, juce::UndoManager& undoManager)
{
    auto placement = NodeLookup::placementWithId (project, placementId);

    if (! placement.isValid())
        return noSuchPlacement (placementId);

    auto from = placement.getParent();

    if (! from.hasType (ids::Placements))
        return CommandResult::refused ("The placement is on an automation lane; only a track's own placement can be moved to a track.");

    const auto toTrack = trackOf (project, toTrackId);

    if (! toTrack.isValid())
        return noSuchTrack (toTrackId);

    auto to = toTrack.getChildWithName (ids::Placements);

    const auto extentCheck = checkExtent (to, toStart, PlacementModel (placement).getLength(), placement);

    if (! extentCheck.wasApplied())
        return extentCheck;

    // The node itself goes over, so it is the same placement afterwards and
    // every handle to it stays good. On its own track only the start moves.
    if (to != from)
    {
        from.removeChild (placement, &undoManager);
        to.appendChild (placement, &undoManager);
    }

    placement.setProperty (ids::start, static_cast<juce::int64> (toStart), &undoManager);

    return CommandResult::applied();
}

CommandResult resizePlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t length, juce::UndoManager& undoManager)
{
    auto placement = NodeLookup::placementWithId (project, placementId);

    if (! placement.isValid())
        return noSuchPlacement (placementId);

    const auto extentCheck = checkExtent (placement.getParent(), PlacementModel (placement).getStart(), length, placement);

    if (! extentCheck.wasApplied())
        return extentCheck;

    placement.setProperty (ids::length, static_cast<juce::int64> (length), &undoManager);

    return CommandResult::applied();
}

CommandResult splitPlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t atTick, juce::UndoManager& undoManager)
{
    auto placement = NodeLookup::placementWithId (project, placementId);

    if (! placement.isValid())
        return noSuchPlacement (placementId);

    const PlacementModel whole (placement);
    const auto start = whole.getStart();
    const auto length = whole.getLength();

    // Measured from the start, so that no sum can pass the largest tick.
    if (atTick <= start || atTick - start >= length)
        return CommandResult::refused ("The placement covers the ticks from " + juce::String (start) + " up to " + juce::String (endOf (whole)) + "; it can be split only at a tick after its start and before its end, not at " + juce::String (atTick) + ".");

    const auto lengthOfFirst = atTick - start;

    const auto clip = clipOf (project, whole.getClipId());

    if (! clip.isValid())
        return CommandResult::refused ("The placement references a clip that is not in the pool.");

    const auto offsetOfSecond = phaseAfter (whole.getOffset(), lengthOfFirst, ClipModel (clip).getLoopLength());
    const auto id = juce::Uuid().toString();

    juce::ValueTree second (ids::Placement);
    second.setProperty (ids::id, id, nullptr);
    second.setProperty (ids::clipId, whole.getClipId(), nullptr);
    second.setProperty (ids::start, static_cast<juce::int64> (atTick), nullptr);
    second.setProperty (ids::length, static_cast<juce::int64> (length - lengthOfFirst), nullptr);

    // An offset of 0 is the default and is not stored, as on a placement
    // that was never split.
    if (offsetOfSecond != 0)
        second.setProperty (ids::offset, static_cast<juce::int64> (offsetOfSecond), nullptr);

    auto parent = placement.getParent();

    placement.setProperty (ids::length, static_cast<juce::int64> (lengthOfFirst), &undoManager);
    parent.addChild (second, parent.indexOf (placement) + 1, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

CommandResult removePlacement (juce::ValueTree project, const juce::String& placementId, juce::UndoManager& undoManager)
{
    const auto placement = NodeLookup::placementWithId (project, placementId);

    if (! placement.isValid())
        return noSuchPlacement (placementId);

    placement.getParent().removeChild (placement, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
