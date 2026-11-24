#include "model/Validator.h"

#include "model/ColourText.h"
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

            // An end past the largest tick is a violation of invariant 8.
            // Here it is held at the largest tick, so that the comparison
            // of such a file cannot overflow the sum.
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

    // Clips by their id. A clip without an id is left out: a reference
    // without a clipId must not find it.
    std::map<juce::String, juce::ValueTree> clipsByIdOf (const juce::ValueTree& project)
    {
        std::map<juce::String, juce::ValueTree> clipsById;

        for (const auto& clip : clipsOf (project))
            if (const auto id = PropertyReader::readString (clip, ids::id); id.isValid())
                clipsById.emplace (id.value, clip);

        return clipsById;
    }

    void checkPlacementOffsets (const juce::ValueTree& project, Violations& violations)
    {
        const auto clipsById = clipsByIdOf (project);

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

    //==========================================================================
    // Shared by the checks of invariants 7 to 12.

    // The value of a property that another check already reports when it is
    // missing or malformed. One fault is one violation.
    template <typename Value>
    std::optional<Value> valueIfValid (const PropertyReader::Result<Value>& stored)
    {
        if (stored.isValid())
            return stored.value;

        return std::nullopt;
    }

    // Nothing to check when there is no value: whoever read it has said why.
    void checkWithin (const juce::ValueTree& node, const juce::Identifier& property, std::optional<std::int64_t> value, std::int64_t lowest, std::int64_t highest, Violations& violations)
    {
        if (! value.has_value())
            return;

        if (*value < lowest)
            addViolation (violations, node, property, "is " + juce::String (*value) + "; the lowest allowed is " + juce::String (lowest));
        else if (*value > highest)
            addViolation (violations, node, property, "is " + juce::String (*value) + "; the highest allowed is " + juce::String (highest));
    }

    // For an int that has no upper limit of its own.
    const std::int64_t largestInt = std::numeric_limits<int>::max();
    const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

    // An int the schema lists without a default.
    void checkRequiredInt (const juce::ValueTree& node, const juce::Identifier& property, std::int64_t lowest, std::int64_t highest, Violations& violations)
    {
        checkWithin (node, property, requiredValue (violations, node, property, PropertyReader::readInt (node, property)), lowest, highest, violations);
    }

    // An int with a default. Every default of the schema lies inside its
    // range, so an absent property needs no look.
    void checkOptionalInt (const juce::ValueTree& node, const juce::Identifier& property, std::int64_t lowest, std::int64_t highest, Violations& violations)
    {
        const auto stored = PropertyReader::readInt (node, property);

        if (stored.isMalformed())
            addMalformed (violations, node, property);
        else if (stored.isValid())
            checkWithin (node, property, stored.value, lowest, highest, violations);
    }

    // A tick the schema lists without a default: there, and not negative.
    void checkRequiredTick (const juce::ValueTree& node, const juce::Identifier& property, Violations& violations)
    {
        checkWithin (node, property, requiredValue (violations, node, property, PropertyReader::readInt64 (node, property)), 0, largestTick, violations);
    }

    // A tick with a default of 0 or more.
    void checkOptionalTick (const juce::ValueTree& node, const juce::Identifier& property, Violations& violations)
    {
        const auto stored = PropertyReader::readInt64 (node, property);

        if (stored.isMalformed())
            addMalformed (violations, node, property);
        else if (stored.isValid())
            checkWithin (node, property, stored.value, 0, largestTick, violations);
    }

    // A bool can only be wrong in its text.
    void checkBool (const juce::ValueTree& node, const juce::Identifier& property, Violations& violations)
    {
        if (PropertyReader::readBool (node, property).isMalformed())
            addMalformed (violations, node, property);
    }

    // The name of a Performer or a Player: there, and not empty [D22].
    void checkPortName (const juce::ValueTree& port, Violations& violations)
    {
        const auto stored = PropertyReader::readString (port, ids::name);

        if (stored.isAbsent())
            addViolation (violations, port, ids::name, "is required and missing");
        else if (stored.value.isEmpty())
            addViolation (violations, port, ids::name, "is empty; a " + port.getType().toString() + " has a name");
    }

    // A string that, when present, is one of a list.
    void checkListedText (const juce::ValueTree& node, const juce::Identifier& property, const juce::StringArray& allowed, Violations& violations)
    {
        const auto stored = PropertyReader::readString (node, property);

        if (stored.isValid() && ! allowed.contains (stored.value))
            addViolation (violations, node, property, "\"" + stored.value + "\" is not one of: " + allowed.joinIntoString (", "));
    }

    bool isHexadecimal (const juce::String& text)
    {
        return text.containsOnly ("0123456789abcdefABCDEF");
    }

    // "#AARRGGBB", or empty for no colour (docs/SCHEMA.md -> Conventions).
    void checkColour (const juce::ValueTree& node, Violations& violations)
    {
        const auto colour = PropertyReader::readString (node, ids::colour);

        if (colour.isValid() && ! ColourText::isValid (colour.value))
            addViolation (violations, node, ids::colour, "\"" + colour.value + "\" is not a colour: '#' and eight hexadecimal digits");
    }

    // A double holds no more than six significant digits [D14]: it is the
    // value its canonical text, juce::String (double), reads back as. The
    // test is on the value, so "118.0" is 118 and passes [D17].
    void checkSixDigits (const juce::ValueTree& node, const juce::Identifier& property, double value, Violations& violations)
    {
        const auto savedText = juce::String (value);

        if (! juce::exactlyEqual (savedText.getDoubleValue(), value))
            addViolation (violations, node, property, "\"" + node[property].toString() + "\" has more than six significant digits; it would be saved as " + savedText);
    }

    std::vector<juce::ValueTree> notesOf (const juce::ValueTree& clip)
    {
        return itemsOf (clip, ids::Events, ids::Note);
    }

    std::vector<juce::ValueTree> eventsOf (const juce::ValueTree& clip)
    {
        return itemsOf (clip, ids::Events, ids::Event);
    }

    std::vector<juce::ValueTree> gridLanesOf (const juce::ValueTree& clip)
    {
        return itemsOf (clip, ids::Grid, ids::Lane);
    }

    std::vector<juce::ValueTree> padsOf (const juce::ValueTree& project)
    {
        std::vector<juce::ValueTree> pads;

        for (const auto& routing : childrenOfType (project, ids::Routing))
            for (const auto& pad : itemsOf (routing, ids::PadMap, ids::Pad))
                pads.push_back (pad);

        return pads;
    }

    //==========================================================================
    // Invariant 7: Project.ppq == 960.

    void checkPpq (const juce::ValueTree& project, Violations& violations)
    {
        const auto ppq = valueOrDefault (violations, project, ids::ppq, PropertyReader::readInt (project, ids::ppq), 960);

        if (ppq.has_value() && *ppq != 960)
            addViolation (violations, project, ids::ppq, "is " + juce::String (*ppq) + "; Hums works in 960 ticks per quarter note and in nothing else");
    }

    //==========================================================================
    // Invariant 8: every event clip has a loopLength > 0; a step clip has
    // neither loopLength nor quantise; every Note has a length > 0; no tick
    // anywhere is negative, and no Note or Placement ends past the largest
    // tick there is.
    //
    // A placement's offset is a time too, but invariant 4 judges it, and the
    // times of UiState and Session are with invariant 10.

    // A Note or a Placement that begins at this tick with this length ends
    // past the largest tick an int64 holds. The commands refuse to make one,
    // and whoever adds the two would overflow. Asked only of a tick and a
    // length that are valid in themselves.
    bool endsPastTheLargestTick (std::int64_t tick, std::int64_t length)
    {
        return tick >= 0 && length > 0 && length > largestTick - tick;
    }

    juce::String reasonForAnEndPastTheLargestTick (const juce::Identifier& tickProperty, std::int64_t tick, std::int64_t length)
    {
        return "is " + juce::String (length) + " with a " + tickProperty.toString() + " of " + juce::String (tick) + "; that ends past the largest tick there is, " + juce::String (largestTick);
    }

    void checkLoopLengthsAndTicks (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& timeline : childrenOfType (project, ids::Timeline))
        {
            // Invariant 6 reports a tempo or time signature without a tick.
            for (const auto& eventType : { ids::TempoEvent, ids::TimeSigEvent })
                for (const auto& event : childrenOfType (timeline, eventType))
                    checkWithin (event, ids::tick, valueIfValid (PropertyReader::readInt64 (event, ids::tick)), 0, largestTick, violations);

            for (const auto& marker : childrenOfType (timeline, ids::Marker))
                checkRequiredTick (marker, ids::tick, violations);
        }

        for (const auto& clip : clipsOf (project))
        {
            if (isEventClip (clip))
            {
                const auto loopLength = requiredValue (violations, clip, ids::loopLength, PropertyReader::readInt64 (clip, ids::loopLength));

                if (loopLength.has_value() && *loopLength <= 0)
                    addViolation (violations, clip, ids::loopLength, "is " + juce::String (*loopLength) + "; the loop of an event clip is at least one tick long");
            }
            else if (isStepClip (clip))
            {
                // The loop of a step clip follows from its Grid; a stored
                // length would be a second source of truth.
                for (const auto& property : { ids::loopLength, ids::quantise })
                    if (clip.hasProperty (property))
                        addViolation (violations, clip, property, "is for event clips only; a step clip must not have it");
            }

            for (const auto& note : notesOf (clip))
            {
                const auto tick = requiredValue (violations, note, ids::tick, PropertyReader::readInt64 (note, ids::tick));
                checkWithin (note, ids::tick, tick, 0, largestTick, violations);

                const auto length = requiredValue (violations, note, ids::length, PropertyReader::readInt64 (note, ids::length));

                if (length.has_value() && *length <= 0)
                    addViolation (violations, note, ids::length, "is " + juce::String (*length) + "; a Note is at least one tick long");
                else if (tick.has_value() && length.has_value() && endsPastTheLargestTick (*tick, *length))
                    addViolation (violations, note, ids::length, reasonForAnEndPastTheLargestTick (ids::tick, *tick, *length));
            }

            for (const auto& event : eventsOf (clip))
                checkRequiredTick (event, ids::tick, violations);
        }

        // Invariant 3 reports a placement without a start or a length.
        for (const auto& track : tracksOf (project))
        {
            for (const auto& placement : allPlacementsOf (track))
            {
                const auto start = valueIfValid (PropertyReader::readInt64 (placement, ids::start));
                checkWithin (placement, ids::start, start, 0, largestTick, violations);

                const auto length = PropertyReader::readInt64 (placement, ids::length);

                if (length.isValid() && length.value <= 0)
                    addViolation (violations, placement, ids::length, "is " + juce::String (length.value) + "; a Placement is at least one tick long");
                else if (start.has_value() && length.isValid() && endsPastTheLargestTick (*start, length.value))
                    addViolation (violations, placement, ids::length, reasonForAnEndPastTheLargestTick (ids::start, *start, length.value));
            }
        }
    }

    //==========================================================================
    // Invariant 9: a clip referenced by any AutomationLane placement contains
    // only cc events. That makes it an event clip without notes. And it is
    // on lanes only: no placement of a track itself and no slot references
    // it [D21].

    bool isOtherThanCc (const juce::String& eventKind)
    {
        return juce::StringArray { "pitchBend", "programChange", "channelPressure", "polyPressure", "sysex" }.contains (eventKind);
    }

    void checkAutomationClips (const juce::ValueTree& project, Violations& violations)
    {
        const auto clipsById = clipsByIdOf (project);

        // A clip that several lane placements reference is reported once.
        std::set<juce::String> clipIdsLookedAt;

        // Every event clip a lane placement references. A step clip on a
        // lane is reported there, at the lane's placement, and not again at
        // its other references.
        std::set<juce::String> clipIdsOnLanes;

        for (const auto& track : tracksOf (project))
        {
            for (const auto& lane : automationLanesOf (track))
            {
                for (const auto& placement : childrenOfType (lane, ids::Placement))
                {
                    const auto clipId = PropertyReader::readString (placement, ids::clipId);
                    const auto clip = clipId.isValid() ? clipsById.find (clipId.value) : clipsById.end();

                    // A reference that names no clip is invariant 1's finding.
                    if (clip == clipsById.end())
                        continue;

                    if (isStepClip (clip->second))
                    {
                        addViolation (violations, placement, ids::clipId, "names a step clip; an AutomationLane takes only an event clip that holds nothing but cc events");
                        continue;
                    }

                    clipIdsOnLanes.insert (clipId.value);

                    const auto isFirstLook = clipIdsLookedAt.insert (clipId.value).second;

                    if (! isFirstLook)
                        continue;

                    for (const auto& note : notesOf (clip->second))
                        addNodeViolation (violations, note, "a Note in a clip that an AutomationLane references; such a clip holds only cc events");

                    // An event of no known kind is invariant 10's finding.
                    for (const auto& event : eventsOf (clip->second))
                        if (const auto kind = PropertyReader::readString (event, ids::kind); isOtherThanCc (kind.value))
                            addViolation (violations, event, ids::kind, "is \"" + kind.value + "\" in a clip that an AutomationLane references; such a clip holds only cc events");
                }
            }
        }

        // The other references of those clips: each is one too many.
        for (const auto& track : tracksOf (project))
        {
            auto references = itemsOf (track, ids::Placements, ids::Placement);
            const auto slots = itemsOf (track, ids::Slots, ids::Slot);
            references.insert (references.end(), slots.begin(), slots.end());

            for (const auto& reference : references)
                if (const auto clipId = PropertyReader::readString (reference, ids::clipId); clipId.isValid() && clipIdsOnLanes.count (clipId.value) > 0)
                    addViolation (violations, reference, ids::clipId, "names a clip that an AutomationLane references; such a clip is on AutomationLanes only");
        }
    }

    //==========================================================================
    // Invariant 10: every value lies in its range or its list. Beyond the
    // values the invariant names, this holds every property to its table in
    // docs/SCHEMA.md -> Node reference: one the schema lists without a
    // default is there, and one that is there is in the text form of its
    // type [D17].

    void checkProjectValues (const juce::ValueTree& project, Violations& violations)
    {
        // Which versions this build reads is the load path's question
        // (io/ProjectFile); here it only has to be a version.
        checkRequiredInt (project, ids::schemaVersion, 1, largestInt, violations);
    }

    void checkRoutingValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& performer : itemsOf (project, ids::Routing, ids::Performer))
        {
            checkPortName (performer, violations);
            checkOptionalInt (performer, ids::channelFilter, 0, 16, violations);
        }

        for (const auto& player : itemsOf (project, ids::Routing, ids::Player))
        {
            checkPortName (player, violations);
            checkBool (player, ids::sendClock, violations);
        }

        for (const auto& pad : padsOf (project))
        {
            checkRequiredInt (pad, ids::index, 0, largestInt, violations);
            checkRequiredInt (pad, ids::inputPitch, 0, 127, violations);
            checkRequiredInt (pad, ids::pitch, 0, 127, violations);
        }
    }

    void checkTimelineValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& tempoEvent : itemsOf (project, ids::Timeline, ids::TempoEvent))
        {
            // Finite it is, or it would not have been read as a double.
            const auto bpm = requiredValue (violations, tempoEvent, ids::bpm, PropertyReader::readDouble (tempoEvent, ids::bpm));

            if (! bpm.has_value())
                continue;

            if (*bpm <= 0.0)
                addViolation (violations, tempoEvent, ids::bpm, "is " + juce::String (*bpm) + "; a tempo is greater than 0");
            else
                checkSixDigits (tempoEvent, ids::bpm, *bpm, violations);
        }

        for (const auto& timeSigEvent : itemsOf (project, ids::Timeline, ids::TimeSigEvent))
        {
            checkRequiredInt (timeSigEvent, ids::numerator, 1, 32, violations);

            const auto denominator = requiredValue (violations, timeSigEvent, ids::denominator, PropertyReader::readInt (timeSigEvent, ids::denominator));
            const auto isPowerOfTwoUpTo64 = [] (int number) { return number >= 1 && number <= 64 && (number & (number - 1)) == 0; };

            if (denominator.has_value() && ! isPowerOfTwoUpTo64 (*denominator))
                addViolation (violations, timeSigEvent, ids::denominator, "is " + juce::String (*denominator) + "; it is one of 1, 2, 4, 8, 16, 32, 64");
        }
    }

    void checkNoteValues (const juce::ValueTree& note, Violations& violations)
    {
        checkRequiredInt (note, ids::pitch, 0, 127, violations);
        checkOptionalInt (note, ids::velocity, 1, 127, violations);
        checkOptionalInt (note, ids::offVelocity, 0, 127, violations);
        checkBool (note, ids::enabled, violations);
    }

    // An a or b that the event's kind has no use for: 0, which is also what
    // an absent one reads as.
    void checkUnused (const juce::ValueTree& event, const juce::Identifier& property, std::optional<std::int64_t> value, const juce::String& kind, Violations& violations)
    {
        if (value.has_value() && *value != 0)
            addViolation (violations, event, property, "is " + juce::String (*value) + "; a " + kind + " event has no " + property.toString() + ", it is 0");
    }

    void checkEventValues (const juce::ValueTree& event, Violations& violations)
    {
        const auto kind = requiredValue (violations, event, ids::kind, PropertyReader::readString (event, ids::kind));
        const auto a = valueOrDefault (violations, event, ids::a, PropertyReader::readInt (event, ids::a), 0);
        const auto b = valueOrDefault (violations, event, ids::b, PropertyReader::readInt (event, ids::b), 0);

        checkBool (event, ids::enabled, violations);

        if (! kind.has_value())
            return;

        // The table of docs/SCHEMA.md -> Event. An event carries only what
        // its kind uses: an a or b the kind has no use for is 0, and only a
        // sysex event has data (invariant 10). That is what the commands
        // make, so anything else is nothing Hums wrote [D17].
        const auto data = PropertyReader::readString (event, ids::data);
        const auto hasData = data.isValid() && data.value.isNotEmpty();

        if (*kind == "cc" || *kind == "polyPressure")
        {
            checkWithin (event, ids::a, a, 0, 127, violations);
            checkWithin (event, ids::b, b, 0, 127, violations);
        }
        else if (*kind == "pitchBend")
        {
            checkWithin (event, ids::a, a, 0, 16383, violations);
            checkUnused (event, ids::b, b, *kind, violations);
        }
        else if (*kind == "programChange" || *kind == "channelPressure")
        {
            checkWithin (event, ids::a, a, 0, 127, violations);
            checkUnused (event, ids::b, b, *kind, violations);
        }
        else if (*kind == "sysex")
        {
            checkUnused (event, ids::a, a, *kind, violations);
            checkUnused (event, ids::b, b, *kind, violations);

            // The payload, without F0 and F7. At most 64 KB, and a byte is
            // two digits.
            const int mostDataDigits = 2 * 64 * 1024;

            if (! hasData)
                return;

            if (! isHexadecimal (data.value))
                addViolation (violations, event, ids::data, "holds a character that is no hexadecimal digit");
            else if (data.value.length() % 2 != 0)
                addViolation (violations, event, ids::data, "has " + juce::String (data.value.length()) + " hexadecimal digits; a byte is two of them");
            else if (data.value.length() > mostDataDigits)
                addViolation (violations, event, ids::data, "holds " + juce::String (data.value.length() / 2) + " bytes; the most allowed is 65536");

            return;
        }
        else
        {
            addViolation (violations, event, ids::kind, "\"" + *kind + "\" is not one of: cc, pitchBend, programChange, channelPressure, polyPressure, sysex");
            return;
        }

        if (hasData)
            addViolation (violations, event, ids::data, "is there; only a sysex event has data, not a " + *kind + " event");
    }

    void checkGridValues (const juce::ValueTree& grid, Violations& violations)
    {
        // An int64 like every length, though its range is small.
        const auto stepLength = PropertyReader::readInt64 (grid, ids::stepLength);

        if (stepLength.isMalformed())
            addMalformed (violations, grid, ids::stepLength);
        else
            checkWithin (grid, ids::stepLength, valueIfValid (stepLength), 1, 3840, violations);

        // Invariant 2 reports a stepCount that is malformed or below 1.
        const auto stepCount = PropertyReader::readInt (grid, ids::stepCount);

        if (stepCount.isValid() && stepCount.value > 64)
            addViolation (violations, grid, ids::stepCount, "is " + juce::String (stepCount.value) + "; the highest allowed is 64");

        for (const auto& lane : childrenOfType (grid, ids::Lane))
        {
            // No default: a lane without it is a melodic row.
            checkOptionalInt (lane, ids::fixedPitch, 0, 127, violations);

            for (const auto& cell : childrenOfType (lane, ids::Cell))
            {
                checkOptionalInt (cell, ids::pitch, 0, 127, violations);
                checkBool (cell, ids::on, violations);
                checkOptionalInt (cell, ids::velocity, 1, 127, violations);

                const auto gate = PropertyReader::readDouble (cell, ids::gate);

                if (gate.isMalformed())
                    addMalformed (violations, cell, ids::gate);
                else if (gate.isValid() && (gate.value <= 0.0 || gate.value > 4.0))
                    addViolation (violations, cell, ids::gate, "is " + juce::String (gate.value) + "; a gate is greater than 0 and at most 4");
                else if (gate.isValid())
                    checkSixDigits (cell, ids::gate, gate.value, violations);

                for (const auto& ccValue : childrenOfType (cell, ids::CcValue))
                {
                    checkRequiredInt (ccValue, ids::cc, 0, 127, violations);
                    checkRequiredInt (ccValue, ids::value, 0, 127, violations);
                }
            }
        }
    }

    void checkClipValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& clip : clipsOf (project))
        {
            checkColour (clip, violations);

            // On a step clip invariant 8 refuses the property as such.
            if (isEventClip (clip))
                checkListedText (clip, ids::quantise, { "", "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T" }, violations);

            for (const auto& note : notesOf (clip))
                checkNoteValues (note, violations);

            for (const auto& event : eventsOf (clip))
                checkEventValues (event, violations);

            for (const auto& grid : childrenOfType (clip, ids::Grid))
                checkGridValues (grid, violations);
        }
    }

    void checkTrackValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& track : tracksOf (project))
        {
            checkColour (track, violations);
            checkOptionalInt (track, ids::channel, 1, 16, violations);
            checkBool (track, ids::mute, violations);
            checkBool (track, ids::solo, violations);

            for (const auto& knob : itemsOf (track, ids::Knobs, ids::Knob))
                checkRequiredInt (knob, ids::cc, 0, 127, violations);

            for (const auto& slot : itemsOf (track, ids::Slots, ids::Slot))
                checkRequiredInt (slot, ids::scene, 0, largestInt, violations);
        }
    }

    void checkSessionValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& session : childrenOfType (project, ids::Session))
        {
            checkOptionalTick (session, ids::launchQuantise, violations);

            for (const auto& scene : childrenOfType (session, ids::Scene))
                checkRequiredInt (scene, ids::index, 0, largestInt, violations);
        }
    }

    // UiState's own properties [D17]. The view-state children are not looked
    // into (docs/SCHEMA.md -> UiState).
    void checkUiStateValues (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& uiState : childrenOfType (project, ids::UiState))
        {
            checkListedText (uiState, ids::activeTab, { "routing", "pianoroll", "arrangement", "drums", "stepseq", "automation" }, violations);
            checkListedText (uiState, ids::sharedEditPolicy, { "updateAll", "makeUnique" }, violations);
            checkOptionalTick (uiState, ids::loopStart, violations);
            checkOptionalTick (uiState, ids::loopEnd, violations);
            checkBool (uiState, ids::loopEnabled, violations);
            checkOptionalTick (uiState, ids::exportLength, violations);

            const auto loopStart = PropertyReader::readInt64 (uiState, ids::loopStart);
            const auto loopEnd = PropertyReader::readInt64 (uiState, ids::loopEnd);
            const auto loopEnabled = PropertyReader::readBool (uiState, ids::loopEnabled);

            // Only a loop whose three properties are in form can contradict
            // itself. An absent start or end is 0.
            if (loopStart.isMalformed() || loopEnd.isMalformed() || ! loopEnabled.isValid() || ! loopEnabled.value)
                continue;

            const std::int64_t start = loopStart.isValid() ? loopStart.value : 0;
            const std::int64_t end = loopEnd.isValid() ? loopEnd.value : 0;

            if (start >= end)
                addViolation (violations, uiState, ids::loopEnabled, "the loop is enabled, but its start (" + juce::String (start) + ") is not before its end (" + juce::String (end) + ")");
        }
    }

    void checkValues (const juce::ValueTree& project, Violations& violations)
    {
        checkProjectValues (project, violations);
        checkRoutingValues (project, violations);
        checkTimelineValues (project, violations);
        checkClipValues (project, violations);
        checkTrackValues (project, violations);
        checkSessionValues (project, violations);
        checkUiStateValues (project, violations);
    }

    //==========================================================================
    // Invariant 11: index-keyed children are unique in their scope. So are
    // the names of the Performers, and those of the Players [D22].

    // The nodes are those of one scope. A key that is missing or malformed
    // is invariant 10's finding and takes no part.
    void checkUniqueKey (const std::vector<juce::ValueTree>& nodes, const juce::Identifier& key, Violations& violations)
    {
        std::map<int, juce::ValueTree> firstNodeWithKey;

        for (const auto& node : nodes)
        {
            const auto stored = PropertyReader::readInt (node, key);

            if (! stored.isValid())
                continue;

            const auto [holder, isFirst] = firstNodeWithKey.emplace (stored.value, node);

            if (! isFirst)
                addViolation (violations, node, key, "is " + juce::String (stored.value) + ", and so is that of " + pathOf (holder->second));
        }
    }

    // The nodes are the ports of one kind. A name that is missing is
    // invariant 10's finding and takes no part; nor does an empty one, which
    // is refused there as well. Compared as written.
    void checkUniqueNames (const std::vector<juce::ValueTree>& ports, Violations& violations)
    {
        std::map<juce::String, juce::ValueTree> firstPortWithName;

        for (const auto& port : ports)
        {
            const auto stored = PropertyReader::readString (port, ids::name);

            if (! stored.isValid() || stored.value.isEmpty())
                continue;

            const auto [holder, isFirst] = firstPortWithName.emplace (stored.value, port);

            if (! isFirst)
                addViolation (violations, port, ids::name, "is \"" + stored.value + "\", and so is that of " + pathOf (holder->second));
        }
    }

    void checkUniqueKeys (const juce::ValueTree& project, Violations& violations)
    {
        checkUniqueNames (itemsOf (project, ids::Routing, ids::Performer), violations);
        checkUniqueNames (itemsOf (project, ids::Routing, ids::Player), violations);
        checkUniqueKey (padsOf (project), ids::index, violations);
        checkUniqueKey (padsOf (project), ids::inputPitch, violations);
        checkUniqueKey (itemsOf (project, ids::Session, ids::Scene), ids::index, violations);

        for (const auto& track : tracksOf (project))
        {
            checkUniqueKey (itemsOf (track, ids::Slots, ids::Slot), ids::scene, violations);
            checkUniqueKey (itemsOf (track, ids::Knobs, ids::Knob), ids::cc, violations);
        }

        for (const auto& clip : clipsOf (project))
            for (const auto& lane : gridLanesOf (clip))
                for (const auto& cell : childrenOfType (lane, ids::Cell))
                    checkUniqueKey (childrenOfType (cell, ids::CcValue), ids::cc, violations);
    }

    //==========================================================================
    // Invariant 12: Project has exactly one of each required container, and
    // every Track exactly one Placements, AutomationLanes, Knobs and Slots.
    //
    // Two containers the invariant does not name are held to the same
    // standard, because the tree of docs/SCHEMA.md has each exactly once and
    // the wrappers count on it: the PadMap of Routing and the Events of an
    // event clip. (The Grid of a step clip is with invariant 2.)

    void checkExactlyOne (const juce::ValueTree& parent, const juce::Identifier& childType, Violations& violations)
    {
        const auto count = childrenOfType (parent, childType).size();

        if (count != 1)
            addNodeViolation (violations, parent, "has " + juce::String (static_cast<std::int64_t> (count)) + " " + childType.toString() + " children; exactly one is required");
    }

    void checkContainers (const juce::ValueTree& project, Violations& violations)
    {
        for (const auto& container : { ids::Routing, ids::Timeline, ids::ClipPool, ids::Tracks, ids::Session, ids::UiState })
            checkExactlyOne (project, container, violations);

        for (const auto& routing : childrenOfType (project, ids::Routing))
            checkExactlyOne (routing, ids::PadMap, violations);

        for (const auto& clip : clipsOf (project))
            if (isEventClip (clip))
                checkExactlyOne (clip, ids::Events, violations);

        for (const auto& track : tracksOf (project))
            for (const auto& container : { ids::Placements, ids::AutomationLanes, ids::Knobs, ids::Slots })
                checkExactlyOne (track, container, violations);
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
    checkPpq (project, violations);
    checkLoopLengthsAndTicks (project, violations);
    checkAutomationClips (project, violations);
    checkValues (project, violations);
    checkUniqueKeys (project, violations);
    checkContainers (project, violations);

    return violations;
}

} // namespace model
