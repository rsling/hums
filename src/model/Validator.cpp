#include "model/Validator.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>

namespace model
{

namespace
{

    using Violations = std::vector<Validator::Violation>;

    //==========================================================================
    // Where a node is.

    // The node types the schema allows several times in one parent (the
    // starred ones in docs/SCHEMA.md -> Tree). Only these get a position in
    // a node path.
    bool canOccurSeveralTimes (const juce::Identifier& type)
    {
        for (const auto& repeatable : { ids::Performer, ids::Player, ids::Pad, ids::TempoEvent, ids::TimeSigEvent, ids::Marker, ids::Clip, ids::Note, ids::Event, ids::Lane, ids::Cell, ids::CcValue, ids::Track, ids::Placement, ids::AutomationLane, ids::Knob, ids::Slot, ids::Scene })
            if (type == repeatable)
                return true;

        return false;
    }

    // The node's position among its siblings of the same type, from 0.
    int positionAmongSiblings (const juce::ValueTree& node)
    {
        int position = 0;

        for (const auto& sibling : node.getParent())
        {
            if (sibling == node)
                break;

            if (sibling.hasType (node.getType()))
                ++position;
        }

        return position;
    }

    // The form is described at Validator::Violation::nodePath. It is worked
    // out only when there is a violation to report.
    juce::String pathOf (const juce::ValueTree& node)
    {
        auto step = node.getType().toString();
        const auto parent = node.getParent();

        if (! parent.isValid())
            return step;

        if (canOccurSeveralTimes (node.getType()))
            step << '[' << positionAmongSiblings (node) << ']';

        return pathOf (parent) + "/" + step;
    }

    //==========================================================================
    // Reporting.

    void addViolation (Violations& violations, const juce::ValueTree& node, const juce::Identifier& property, const juce::String& reason)
    {
        violations.push_back ({ pathOf (node), property.toString(), reason });
    }

    // For a violation that is about the node as a whole.
    void addNodeViolation (Violations& violations, const juce::ValueTree& node, const juce::String& reason)
    {
        violations.push_back ({ pathOf (node), {}, reason });
    }

    void addMalformed (Violations& violations, const juce::ValueTree& node, const juce::Identifier& property)
    {
        addViolation (violations, node, property, "\"" + node[property].toString() + "\" is not in the text form of its type");
    }

    // The value of a property the schema lists without a default. A missing
    // or malformed one is reported, and the caller gets nothing to check.
    template <typename Value>
    std::optional<Value> requiredValue (Violations& violations, const juce::ValueTree& node, const juce::Identifier& property, const PropertyReader::Result<Value>& stored)
    {
        if (stored.isValid())
            return stored.value;

        if (stored.isAbsent())
            addViolation (violations, node, property, "is required and missing");
        else
            addMalformed (violations, node, property);

        return std::nullopt;
    }

    // The value of a property with a default. A malformed one is reported,
    // and the caller gets nothing to check: the default is for an absent
    // property, never a stand-in for a broken one [D17].
    template <typename Value>
    std::optional<Value> valueOrDefault (Violations& violations, const juce::ValueTree& node, const juce::Identifier& property, const PropertyReader::Result<Value>& stored, const Value& valueWhenAbsent)
    {
        if (stored.isValid())
            return stored.value;

        if (stored.isAbsent())
            return valueWhenAbsent;

        addMalformed (violations, node, property);
        return std::nullopt;
    }

    //==========================================================================
    // Walking the tree of docs/SCHEMA.md. Only children of the asked type
    // are returned, so an unknown child never shifts or joins anything.

    std::vector<juce::ValueTree> childrenOfType (const juce::ValueTree& parent, const juce::Identifier& type)
    {
        std::vector<juce::ValueTree> children;

        for (const auto& child : parent)
            if (child.hasType (type))
                children.push_back (child);

        return children;
    }

    // The items inside a container child: the Clips in the ClipPool, the
    // Tracks in Tracks. Should a container be there twice (which invariant
    // 12 refuses), the items of both are returned, so that nothing in the
    // document escapes the other checks.
    std::vector<juce::ValueTree> itemsOf (const juce::ValueTree& parent, const juce::Identifier& containerType, const juce::Identifier& itemType)
    {
        std::vector<juce::ValueTree> items;

        for (const auto& container : childrenOfType (parent, containerType))
            for (const auto& item : childrenOfType (container, itemType))
                items.push_back (item);

        return items;
    }

    std::vector<juce::ValueTree> clipsOf (const juce::ValueTree& project)
    {
        return itemsOf (project, ids::ClipPool, ids::Clip);
    }

    std::vector<juce::ValueTree> tracksOf (const juce::ValueTree& project)
    {
        return itemsOf (project, ids::Tracks, ids::Track);
    }

    std::vector<juce::ValueTree> automationLanesOf (const juce::ValueTree& track)
    {
        return itemsOf (track, ids::AutomationLanes, ids::AutomationLane);
    }

    // Every placement under one track: its own, then those of its
    // automation lanes.
    std::vector<juce::ValueTree> allPlacementsOf (const juce::ValueTree& track)
    {
        auto placements = itemsOf (track, ids::Placements, ids::Placement);

        for (const auto& lane : automationLanesOf (track))
            for (const auto& placement : childrenOfType (lane, ids::Placement))
                placements.push_back (placement);

        return placements;
    }

    bool isStepClip (const juce::ValueTree& clip)
    {
        return PropertyReader::readString (clip, ids::kind).value == "step";
    }

    bool isEventClip (const juce::ValueTree& clip)
    {
        return PropertyReader::readString (clip, ids::kind).value == "event";
    }

    //==========================================================================
    // Invariant 1: ids are valid UUIDs and unique across the project; every
    // reference is empty (where allowed) or names a node of the right type.

    // The text juce::Uuid::toString() writes: 32 hexadecimal digits, lower
    // case, no dashes. juce::Uuid itself would read almost any text, and
    // references are compared as text, so nothing looser is let in.
    bool isUuid (const juce::String& text)
    {
        return text.length() == 32 && text.containsOnly ("0123456789abcdef");
    }

    // Returns the node's id, if it has one, whatever else is wrong with it.
    std::optional<juce::String> checkId (const juce::ValueTree& node, std::map<juce::String, juce::ValueTree>& firstNodeWithId, Violations& violations)
    {
        const auto id = requiredValue (violations, node, ids::id, PropertyReader::readString (node, ids::id));

        if (! id.has_value())
            return std::nullopt;

        if (! isUuid (*id))
            addViolation (violations, node, ids::id, "\"" + *id + "\" is not a UUID: 32 hexadecimal digits in lower case");

        const auto [holder, isFirst] = firstNodeWithId.emplace (*id, node);

        if (! isFirst)
            addViolation (violations, node, ids::id, "is also the id of " + pathOf (holder->second));

        return id;
    }

    // A reference that must name a node.
    void checkRequiredReference (const juce::ValueTree& node, const juce::Identifier& property, const std::set<juce::String>& idsOfTargets, const juce::Identifier& targetType, Violations& violations)
    {
        const auto reference = requiredValue (violations, node, property, PropertyReader::readString (node, property));

        if (reference.has_value() && ! idsOfTargets.contains (*reference))
            addViolation (violations, node, property, "\"" + *reference + "\" is not the id of a " + targetType.toString());
    }

    // A reference that may be absent or empty: unassigned.
    void checkOptionalReference (const juce::ValueTree& node, const juce::Identifier& property, const std::set<juce::String>& idsOfTargets, const juce::Identifier& targetType, Violations& violations)
    {
        const auto reference = PropertyReader::readString (node, property);

        if (reference.isValid() && reference.value.isNotEmpty() && ! idsOfTargets.contains (reference.value))
            addViolation (violations, node, property, "\"" + reference.value + "\" is not the id of a " + targetType.toString());
    }

    void checkIdsAndReferences (const juce::ValueTree& project, Violations& violations)
    {
        std::map<juce::String, juce::ValueTree> firstNodeWithId;
        std::set<juce::String> performerIds;
        std::set<juce::String> playerIds;
        std::set<juce::String> clipIds;

        for (const auto& performer : itemsOf (project, ids::Routing, ids::Performer))
            if (const auto id = checkId (performer, firstNodeWithId, violations))
                performerIds.insert (*id);

        for (const auto& player : itemsOf (project, ids::Routing, ids::Player))
            if (const auto id = checkId (player, firstNodeWithId, violations))
                playerIds.insert (*id);

        for (const auto& clip : clipsOf (project))
        {
            if (const auto id = checkId (clip, firstNodeWithId, violations))
                clipIds.insert (*id);

            for (const auto& lane : itemsOf (clip, ids::Grid, ids::Lane))
                checkId (lane, firstNodeWithId, violations);
        }

        for (const auto& track : tracksOf (project))
        {
            checkId (track, firstNodeWithId, violations);
            checkOptionalReference (track, ids::playerId, playerIds, ids::Player, violations);
            checkOptionalReference (track, ids::performerId, performerIds, ids::Performer, violations);

            for (const auto& lane : automationLanesOf (track))
                checkId (lane, firstNodeWithId, violations);

            for (const auto& placement : allPlacementsOf (track))
            {
                checkId (placement, firstNodeWithId, violations);
                checkRequiredReference (placement, ids::clipId, clipIds, ids::Clip, violations);
            }

            for (const auto& slot : itemsOf (track, ids::Slots, ids::Slot))
                checkRequiredReference (slot, ids::clipId, clipIds, ids::Clip, violations);
        }
    }

    //==========================================================================
    // Invariant 2: the shape of a Grid.

    // The columns that take time in one pass of the loop: those before
    // resetAt that are not skipped. Nothing when stepCount, resetAt or
    // skipMask is malformed or out of shape; checkGrid says which.
    std::optional<int> audibleColumnCountOf (const juce::ValueTree& grid)
    {
        const auto storedStepCount = PropertyReader::readInt (grid, ids::stepCount);
        const auto storedResetAt = PropertyReader::readInt (grid, ids::resetAt);
        const auto storedSkipMask = PropertyReader::readString (grid, ids::skipMask);

        if (storedStepCount.isMalformed() || storedResetAt.isMalformed())
            return std::nullopt;

        const auto stepCount = storedStepCount.isValid() ? storedStepCount.value : 16;
        const auto resetAt = storedResetAt.isValid() ? storedResetAt.value : stepCount;

        if (stepCount < 1 || resetAt < 1 || resetAt > stepCount)
            return std::nullopt;

        // No stored mask: no column is skipped.
        if (! storedSkipMask.isValid())
            return resetAt;

        if (storedSkipMask.value.length() != stepCount || ! storedSkipMask.value.containsOnly ("01"))
            return std::nullopt;

        // One byte per column, now that the mask is known to be '0's and '1's.
        const auto skipMask = storedSkipMask.value.toStdString();
        int audibleColumns = 0;

        for (int column = 0; column < resetAt; ++column)
            if (skipMask[static_cast<std::size_t> (column)] != '1')
                ++audibleColumns;

        return audibleColumns;
    }

    void checkGrid (const juce::ValueTree& grid, Violations& violations)
    {
        const auto stepCount = valueOrDefault (violations, grid, ids::stepCount, PropertyReader::readInt (grid, ids::stepCount), 16);

        // Everything else in a Grid is measured against the step count.
        if (! stepCount.has_value())
            return;

        if (*stepCount < 1)
        {
            addViolation (violations, grid, ids::stepCount, "is " + juce::String (*stepCount) + "; a Grid has at least one column");
            return;
        }

        for (const auto& lane : childrenOfType (grid, ids::Lane))
        {
            const auto cellCount = childrenOfType (lane, ids::Cell).size();

            if (cellCount != static_cast<std::size_t> (*stepCount))
                addNodeViolation (violations, lane, "has " + juce::String (static_cast<std::int64_t> (cellCount)) + " Cell children, the stepCount is " + juce::String (*stepCount));
        }

        const auto resetAt = valueOrDefault (violations, grid, ids::resetAt, PropertyReader::readInt (grid, ids::resetAt), *stepCount);

        if (resetAt.has_value() && (*resetAt < 1 || *resetAt > *stepCount))
            addViolation (violations, grid, ids::resetAt, "is " + juce::String (*resetAt) + ", outside 1 to the stepCount of " + juce::String (*stepCount));

        const auto skipMask = PropertyReader::readString (grid, ids::skipMask);

        if (skipMask.isValid())
        {
            if (skipMask.value.length() != *stepCount)
                addViolation (violations, grid, ids::skipMask, "has " + juce::String (skipMask.value.length()) + " characters, the stepCount is " + juce::String (*stepCount));
            else if (! skipMask.value.containsOnly ("01"))
                addViolation (violations, grid, ids::skipMask, "holds a character other than '0' and '1'");
        }

        // Only a Grid that is in shape so far has a count to judge.
        if (audibleColumnCountOf (grid) == 0)
            addNodeViolation (violations, grid, "every column before resetAt is skipped; one must be left to play");
    }

    void checkGrids (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& clip : clipsOf (project))
        {
            const auto grids = childrenOfType (clip, ids::Grid);

            // Without its Grid a step clip has no lanes, no columns and no
            // loop length, so none of invariant 2 could hold.
            if (isStepClip (clip) && grids.size() != 1)
                addNodeViolation (violations, clip, "a step clip has exactly one Grid, this one has " + juce::String (static_cast<std::int64_t> (grids.size())));

            for (const auto& grid : grids)
                checkGrid (grid, violations);
        }
    }

    //==========================================================================
    // Invariant 3: placements within one parent do not overlap.

    // What a placement covers on the timeline: [start, end).
    struct Extent
    {
        std::int64_t start = 0;
        std::int64_t end = 0;
        juce::ValueTree placement;
    };

    // The placements are those of one parent: one track's own, or one
    // automation lane's.
    void checkOverlapsAmong (const std::vector<juce::ValueTree>& placements, Violations& violations)
    {
        std::vector<Extent> extents;

        for (const auto& placement : placements)
        {
            const auto start = requiredValue (violations, placement, ids::start, PropertyReader::readInt64 (placement, ids::start));
            const auto length = requiredValue (violations, placement, ids::length, PropertyReader::readInt64 (placement, ids::length));

            // A negative start or a length that is not positive is a
            // violation of its own, not an overlap: there is no extent to
            // compare.
            if (! start.has_value() || ! length.has_value() || *start < 0 || *length <= 0)
                continue;

            // An end past the largest tick is held at the largest tick, so
            // that a nonsensical file cannot overflow the sum.
            const auto largestTick = std::numeric_limits<std::int64_t>::max();
            const auto end = *length > largestTick - *start ? largestTick : *start + *length;

            extents.push_back ({ *start, end, placement });
        }

        // Document order is not time order.
        std::stable_sort (extents.begin(), extents.end(), [] (const Extent& first, const Extent& second) { return first.start < second.start; });

        // Going through the placements by start, one overlaps an earlier one
        // exactly when it starts before the furthest end seen so far.
        const Extent* reachingFurthest = nullptr;

        for (const auto& extent : extents)
        {
            if (reachingFurthest != nullptr && extent.start < reachingFurthest->end)
                addViolation (violations, extent.placement, ids::start, "overlaps " + pathOf (reachingFurthest->placement));

            if (reachingFurthest == nullptr || extent.end > reachingFurthest->end)
                reachingFurthest = &extent;
        }
    }

    void checkPlacementOverlaps (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& track : tracksOf (project))
        {
            for (const auto& placements : childrenOfType (track, ids::Placements))
                checkOverlapsAmong (childrenOfType (placements, ids::Placement), violations);

            for (const auto& lane : automationLanesOf (track))
                checkOverlapsAmong (childrenOfType (lane, ids::Placement), violations);
        }
    }

    //==========================================================================
    // Invariant 4: 0 <= offset < loopLength of the referenced clip.

    // The length of one pass of the clip's loop, in ticks: stored on an
    // event clip, step length x audible columns on a step clip. Nothing when
    // the clip does not have a usable one; that is then a violation of
    // another invariant, and no offset can be judged against it.
    std::optional<std::int64_t> loopLengthOf (const juce::ValueTree& clip)
    {
        if (isEventClip (clip))
        {
            const auto loopLength = PropertyReader::readInt64 (clip, ids::loopLength);

            if (loopLength.isValid() && loopLength.value > 0)
                return loopLength.value;

            return std::nullopt;
        }

        if (! isStepClip (clip))
            return std::nullopt;

        const auto grids = childrenOfType (clip, ids::Grid);

        if (grids.size() != 1)
            return std::nullopt;

        const auto audibleColumns = audibleColumnCountOf (grids.front());
        const auto storedStepLength = PropertyReader::readInt64 (grids.front(), ids::stepLength);

        if (! audibleColumns.has_value() || *audibleColumns < 1 || storedStepLength.isMalformed())
            return std::nullopt;

        const std::int64_t stepLength = storedStepLength.isValid() ? storedStepLength.value : 240;

        // The second test keeps a nonsensical step length from overflowing
        // the product.
        if (stepLength < 1 || stepLength > std::numeric_limits<std::int64_t>::max() / *audibleColumns)
            return std::nullopt;

        return stepLength * *audibleColumns;
    }

    void checkPlacementOffsets (const juce::ValueTree& project, Violations& violations)
    {
        std::map<juce::String, juce::ValueTree> clipsById;

        // A clip without an id is left out: a placement without a clipId
        // must not find it.
        for (const auto& clip : clipsOf (project))
            if (const auto id = PropertyReader::readString (clip, ids::id); id.isValid())
                clipsById.emplace (id.value, clip);

        for (const auto& track : tracksOf (project))
        {
            for (const auto& placement : allPlacementsOf (track))
            {
                const auto offset = valueOrDefault<std::int64_t> (violations, placement, ids::offset, PropertyReader::readInt64 (placement, ids::offset), 0);
                const auto clipId = PropertyReader::readString (placement, ids::clipId);
                const auto clip = clipId.isValid() ? clipsById.find (clipId.value) : clipsById.end();

                // A reference that names no clip is invariant 1's finding.
                if (! offset.has_value() || clip == clipsById.end())
                    continue;

                const auto loopLength = loopLengthOf (clip->second);

                if (loopLength.has_value() && (*offset < 0 || *offset >= *loopLength))
                    addViolation (violations, placement, ids::offset, "is " + juce::String (*offset) + ", outside 0 to " + juce::String (*loopLength - 1) + ": the loop of its clip is " + juce::String (*loopLength) + " ticks long");
            }
        }
    }

    //==========================================================================
    // Invariant 5: Clip.kind never changes. That is a statement about a
    // command, which a test makes by comparing a document before and after
    // it. What can be seen in one tree is that every clip has a kind, and
    // that it is one of the two there are.

    void checkClipKinds (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& clip : clipsOf (project))
        {
            const auto kind = requiredValue (violations, clip, ids::kind, PropertyReader::readString (clip, ids::kind));

            if (kind.has_value() && ! isEventClip (clip) && ! isStepClip (clip))
                addViolation (violations, clip, ids::kind, "\"" + *kind + "\" is neither \"event\" nor \"step\"");
        }
    }

    //==========================================================================
    // Invariant 6: a Timeline has a TempoEvent and a TimeSigEvent at tick 0,
    // and at most one of each per tick.

    // For one of the two event types.
    void checkTimelineEvents (const juce::ValueTree& timeline, const juce::Identifier& eventType, Violations& violations)
    {
        std::set<std::int64_t> ticksSeen;

        for (const auto& event : childrenOfType (timeline, eventType))
        {
            const auto tick = requiredValue (violations, event, ids::tick, PropertyReader::readInt64 (event, ids::tick));

            if (! tick.has_value())
                continue;

            const auto isFirstAtItsTick = ticksSeen.insert (*tick).second;

            if (! isFirstAtItsTick)
                addViolation (violations, event, ids::tick, "there is already a " + eventType.toString() + " at tick " + juce::String (*tick));
        }

        if (! ticksSeen.contains (0))
            addNodeViolation (violations, timeline, "has no " + eventType.toString() + " at tick 0");
    }

    void checkTimelines (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& timeline : childrenOfType (project, ids::Timeline))
        {
            checkTimelineEvents (timeline, ids::TempoEvent, violations);
            checkTimelineEvents (timeline, ids::TimeSigEvent, violations);
        }
    }

} // namespace

std::vector<Validator::Violation> Validator::validate (const juce::ValueTree& project)
{
    Violations violations;

    // Nothing below would find anything to look at.
    if (! project.hasType (ids::Project))
    {
        addNodeViolation (violations, project, "the root node is not a Project");
        return violations;
    }

    checkIdsAndReferences (project, violations);
    checkGrids (project, violations);
    checkPlacementOverlaps (project, violations);
    checkPlacementOffsets (project, violations);
    checkClipKinds (project, violations);
    checkTimelines (project, violations);

    return violations;
}

} // namespace model
