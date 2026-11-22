// model::Validator: invariants 1 to 6 of docs/SCHEMA.md -> Invariants.
//
// Every test starts from one valid document, breaks one thing in it and
// expects exactly the violation that belongs to it: the node, the property,
// and nothing beside it.

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "model/Ids.h"
#include "model/Validator.h"

#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;

using Violations = std::vector<model::Validator::Violation>;

// The schema's "Berlin test" example, cut down to one track and given an
// automation lane: a five-step bass placed twice, phase-continuous, a
// filter sweep in a lane over it, and a slot.
juce::ValueTree validDocument()
{
    return juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" name="Berlin test" ppq="960">
             <Routing>
               <Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>
               <Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>
               <PadMap/>
             </Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="118"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
             </Timeline>
             <ClipPool>
               <Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" name="Bass 5-step" kind="step">
                 <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000">
                   <Lane id="1e4b7a0d3c6f49528b5e8a1d4c7f0a3b">
                     <Cell pitch="36" on="1"/><Cell pitch="36" on="1"/><Cell pitch="43" on="1"/><Cell pitch="48"/>
                     <Cell pitch="41" on="1"/><Cell pitch="39" on="1"/><Cell pitch="36" on="1"/><Cell pitch="36" on="1"/>
                   </Lane>
                 </Grid>
               </Clip>
               <Clip id="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" name="Sweep" kind="event" loopLength="1920">
                 <Events>
                   <Event tick="0" kind="cc" a="74" b="20"/>
                   <Event tick="960" kind="cc" a="74" b="90"/>
                 </Events>
               </Clip>
             </ClipPool>
             <Tracks>
               <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass"
                      playerId="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" performerId="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1">
                 <Placements>
                   <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680"/>
                   <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="7680" length="7680" offset="480"/>
                 </Placements>
                 <AutomationLanes>
                   <AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f" name="Cutoff">
                     <Placement id="3c5e7a9c1d2f4b4a8e6c4a2e0d1f3b5c" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="3840" length="1920"/>
                   </AutomationLane>
                 </AutomationLanes>
                 <Knobs/>
                 <Slots>
                   <Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>
                 </Slots>
               </Track>
             </Tracks>
             <Session/>
             <UiState/>
           </Project>)");
}

// An id that no node of the valid document has.
const char* const unusedId = "7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b";

//==============================================================================
// The nodes of the valid document that the tests break.

juce::ValueTree timelineOf (const juce::ValueTree& project)
{
    return project.getChildWithName (ids::Timeline);
}

// 0 is the step clip, 1 the event clip.
juce::ValueTree clipOf (const juce::ValueTree& project, int index)
{
    return project.getChildWithName (ids::ClipPool).getChild (index);
}

juce::ValueTree gridOf (const juce::ValueTree& project)
{
    return clipOf (project, 0).getChildWithName (ids::Grid);
}

juce::ValueTree gridLaneOf (const juce::ValueTree& project)
{
    return gridOf (project).getChildWithName (ids::Lane);
}

juce::ValueTree trackOf (const juce::ValueTree& project)
{
    return project.getChildWithName (ids::Tracks).getChild (0);
}

juce::ValueTree placementsOf (const juce::ValueTree& project)
{
    return trackOf (project).getChildWithName (ids::Placements);
}

juce::ValueTree placementOf (const juce::ValueTree& project, int index)
{
    return placementsOf (project).getChild (index);
}

juce::ValueTree automationLaneOf (const juce::ValueTree& project)
{
    return trackOf (project).getChildWithName (ids::AutomationLanes).getChild (0);
}

juce::ValueTree lanePlacementOf (const juce::ValueTree& project)
{
    return automationLaneOf (project).getChild (0);
}

juce::ValueTree slotOf (const juce::ValueTree& project)
{
    return trackOf (project).getChildWithName (ids::Slots).getChild (0);
}

//==============================================================================
// Breaking things. A property is set as text, which is what a loaded file
// holds.

void set (juce::ValueTree node, const juce::Identifier& property, const juce::String& text)
{
    node.setProperty (property, text, nullptr);
}

void remove (juce::ValueTree node, const juce::Identifier& property)
{
    node.removeProperty (property, nullptr);
}

void append (juce::ValueTree parent, const char* xml)
{
    parent.appendChild (juce::ValueTree::fromXml (xml), nullptr);
}

//==============================================================================
// Reading the result.

std::string describe (const Violations& violations)
{
    juce::String text;

    for (const auto& violation : violations)
        text << violation.nodePath << " | " << violation.property << " | " << violation.reason << "\n";

    return text.toStdString();
}

// The document has exactly one violation, and it is at this node and
// property. An empty property stands for the node as a whole.
void requireOnlyViolation (const juce::ValueTree& project, const char* nodePath, const char* property)
{
    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    REQUIRE (violations.size() == 1);
    CHECK (violations[0].nodePath.toStdString() == nodePath);
    CHECK (violations[0].property.toStdString() == property);
    CHECK (violations[0].reason.isNotEmpty());
}

void requireNoViolation (const juce::ValueTree& project)
{
    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    CHECK (violations.empty());
}

const char* const firstPlacement = "Project/Tracks/Track[0]/Placements/Placement[0]";
const char* const secondPlacement = "Project/Tracks/Track[0]/Placements/Placement[1]";
const char* const lanePlacement = "Project/Tracks/Track[0]/AutomationLanes/AutomationLane[0]/Placement[0]";
const char* const stepClipGrid = "Project/ClipPool/Clip[0]/Grid";

} // namespace

//==============================================================================
// The Validator as a whole.

TEST_CASE ("A valid document has no violations", "[model][Validator]")
{
    requireNoViolation (validDocument());
}

TEST_CASE ("The Validator does not change the tree it looks at", "[model][Validator]")
{
    const auto project = validDocument();
    set (placementOf (project, 1), ids::offset, "5000");
    const auto before = project.toXmlString();

    CHECK_FALSE (model::Validator::validate (project).empty());
    CHECK (project.toXmlString() == before);
}

TEST_CASE ("A tree whose root is not a Project is refused as a whole", "[model][Validator]")
{
    requireOnlyViolation (juce::ValueTree::fromXml ("<Track/>"), "Track", "");
    requireOnlyViolation (juce::ValueTree(), "", "");
}

TEST_CASE ("Unknown properties and children are passed over", "[model][Validator]")
{
    // A newer build's data is preserved, not judged (docs/SCHEMA.md ->
    // Conventions); it may even carry an id that is none.
    const auto project = validDocument();
    set (trackOf (project), "swing", "0.6");
    append (project, R"(<Mixer id="not an id"/>)");
    append (trackOf (project), R"(<Sends id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>)");

    requireNoViolation (project);
}

TEST_CASE ("A violation names its node by position among siblings of its type", "[model][Validator]")
{
    // The unknown child before the second Lane does not count.
    const auto project = validDocument();
    append (gridOf (project), "<Ruler/>");
    append (gridOf (project), R"(<Lane id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b"/>)");

    requireOnlyViolation (project, "Project/ClipPool/Clip[0]/Grid/Lane[1]", "");
}

//==============================================================================
// Invariant 1: ids and references.

TEST_CASE ("Invariant 1: every node that can be referenced needs an id", "[model][Validator]")
{
    SECTION ("a Player")
    {
        // Its track then refers to nothing, which is a second violation.
        const auto project = validDocument();
        remove (project.getChildWithName (ids::Routing).getChildWithName (ids::Player), ids::id);
        remove (trackOf (project), ids::playerId);

        requireOnlyViolation (project, "Project/Routing/Player[0]", "id");
    }

    SECTION ("a Performer")
    {
        const auto project = validDocument();
        remove (project.getChildWithName (ids::Routing).getChildWithName (ids::Performer), ids::id);
        remove (trackOf (project), ids::performerId);

        requireOnlyViolation (project, "Project/Routing/Performer[0]", "id");
    }

    SECTION ("a Lane of a Grid")
    {
        const auto project = validDocument();
        remove (gridLaneOf (project), ids::id);

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]/Grid/Lane[0]", "id");
    }

    SECTION ("a Track")
    {
        const auto project = validDocument();
        remove (trackOf (project), ids::id);

        requireOnlyViolation (project, "Project/Tracks/Track[0]", "id");
    }

    SECTION ("a Placement on a track")
    {
        const auto project = validDocument();
        remove (placementOf (project, 1), ids::id);

        requireOnlyViolation (project, secondPlacement, "id");
    }

    SECTION ("an AutomationLane")
    {
        const auto project = validDocument();
        remove (automationLaneOf (project), ids::id);

        requireOnlyViolation (project, "Project/Tracks/Track[0]/AutomationLanes/AutomationLane[0]", "id");
    }

    SECTION ("a Placement in an automation lane")
    {
        const auto project = validDocument();
        remove (lanePlacementOf (project), ids::id);

        requireOnlyViolation (project, lanePlacement, "id");
    }
}

TEST_CASE ("Invariant 1: a Clip without an id is refused, and so is every reference to it", "[model][Validator]")
{
    const auto project = validDocument();
    remove (clipOf (project, 1), ids::id);

    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    REQUIRE (violations.size() == 2);
    CHECK (violations[0].nodePath == "Project/ClipPool/Clip[1]");
    CHECK (violations[0].property == "id");
    CHECK (violations[1].nodePath == lanePlacement);
    CHECK (violations[1].property == "clipId");
}

TEST_CASE ("Invariant 1: an id is a UUID as Hums writes it", "[model][Validator]")
{
    // juce::Uuid::toString(): 32 hexadecimal digits, lower case, no dashes.
    const auto notAUuid = GENERATE (as<std::string> {},
                                    "",
                                    "track-1",
                                    "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5",    // 31 digits
                                    "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b0",  // 33 digits
                                    "4C6E8A0C2E4F4B1D8A6C4E2A0C1E3F5B",   // upper case
                                    "4c6e8a0c-2e4f-4b1d-8a6c-4e2a0c1e3f5b", // dashed
                                    "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5g");  // not hexadecimal

    const auto project = validDocument();
    set (trackOf (project), ids::id, juce::String (notAUuid));

    requireOnlyViolation (project, "Project/Tracks/Track[0]", "id");
}

TEST_CASE ("Invariant 1: an id is unique across the whole project", "[model][Validator]")
{
    SECTION ("two nodes of one type")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::id, placementOf (project, 0)[ids::id].toString());

        requireOnlyViolation (project, secondPlacement, "id");
    }

    SECTION ("two nodes of different types")
    {
        // The track takes the step clip's id. Clips come first in the
        // document, so the track is the one reported.
        const auto project = validDocument();
        set (trackOf (project), ids::id, clipOf (project, 0)[ids::id].toString());

        const auto violations = model::Validator::validate (project);

        REQUIRE (violations.size() == 1);
        CHECK (violations[0].nodePath == "Project/Tracks/Track[0]");
        CHECK (violations[0].property == "id");
        CHECK (violations[0].reason.contains ("Project/ClipPool/Clip[0]"));
    }
}

TEST_CASE ("Invariant 1: a Track's Player and Performer may be unassigned", "[model][Validator]")
{
    SECTION ("absent")
    {
        const auto project = validDocument();
        remove (trackOf (project), ids::playerId);
        remove (trackOf (project), ids::performerId);

        requireNoViolation (project);
    }

    SECTION ("empty")
    {
        const auto project = validDocument();
        set (trackOf (project), ids::playerId, "");
        set (trackOf (project), ids::performerId, "");

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 1: a playerId names a Player", "[model][Validator]")
{
    SECTION ("an id nothing has")
    {
        const auto project = validDocument();
        set (trackOf (project), ids::playerId, unusedId);

        requireOnlyViolation (project, "Project/Tracks/Track[0]", "playerId");
    }

    SECTION ("the id of a node of another type")
    {
        const auto project = validDocument();
        set (trackOf (project), ids::playerId, trackOf (project)[ids::performerId].toString());

        requireOnlyViolation (project, "Project/Tracks/Track[0]", "playerId");
    }
}

TEST_CASE ("Invariant 1: a performerId names a Performer", "[model][Validator]")
{
    SECTION ("an id nothing has")
    {
        const auto project = validDocument();
        set (trackOf (project), ids::performerId, unusedId);

        requireOnlyViolation (project, "Project/Tracks/Track[0]", "performerId");
    }

    SECTION ("the id of a node of another type")
    {
        const auto project = validDocument();
        set (trackOf (project), ids::performerId, trackOf (project)[ids::playerId].toString());

        requireOnlyViolation (project, "Project/Tracks/Track[0]", "performerId");
    }
}

TEST_CASE ("Invariant 1: a clipId names a Clip and is never empty", "[model][Validator]")
{
    SECTION ("a track placement with an id nothing has")
    {
        const auto project = validDocument();
        set (placementOf (project, 0), ids::clipId, unusedId);

        requireOnlyViolation (project, firstPlacement, "clipId");
    }

    SECTION ("a track placement with the id of a track")
    {
        const auto project = validDocument();
        set (placementOf (project, 0), ids::clipId, trackOf (project)[ids::id].toString());

        requireOnlyViolation (project, firstPlacement, "clipId");
    }

    SECTION ("a track placement with an empty clipId")
    {
        const auto project = validDocument();
        set (placementOf (project, 0), ids::clipId, "");

        requireOnlyViolation (project, firstPlacement, "clipId");
    }

    SECTION ("a track placement without a clipId")
    {
        const auto project = validDocument();
        remove (placementOf (project, 0), ids::clipId);

        requireOnlyViolation (project, firstPlacement, "clipId");
    }

    SECTION ("a placement in an automation lane")
    {
        const auto project = validDocument();
        set (lanePlacementOf (project), ids::clipId, unusedId);

        requireOnlyViolation (project, lanePlacement, "clipId");
    }

    SECTION ("a slot with an id nothing has")
    {
        const auto project = validDocument();
        set (slotOf (project), ids::clipId, unusedId);

        requireOnlyViolation (project, "Project/Tracks/Track[0]/Slots/Slot[0]", "clipId");
    }

    SECTION ("a slot without a clipId")
    {
        const auto project = validDocument();
        remove (slotOf (project), ids::clipId);

        requireOnlyViolation (project, "Project/Tracks/Track[0]/Slots/Slot[0]", "clipId");
    }
}

//==============================================================================
// Invariant 2: the shape of a Grid.

TEST_CASE ("Invariant 2: a Grid with nothing stored and sixteen cells per lane is in shape", "[model][Validator]")
{
    const auto project = validDocument();
    const auto grid = gridOf (project);
    remove (grid, ids::stepLength);
    remove (grid, ids::stepCount);
    remove (grid, ids::resetAt);
    remove (grid, ids::skipMask);

    for (int cell = 8; cell < 16; ++cell)
        append (gridLaneOf (project), "<Cell/>");

    // The loop is now 16 x 240 ticks, so the second placement's offset of
    // 480 still lies inside it.
    requireNoViolation (project);
}

TEST_CASE ("Invariant 2: every lane has exactly stepCount cells", "[model][Validator]")
{
    SECTION ("one too few")
    {
        const auto project = validDocument();
        gridLaneOf (project).removeChild (7, nullptr);

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]/Grid/Lane[0]", "");
    }

    SECTION ("one too many")
    {
        const auto project = validDocument();
        append (gridLaneOf (project), "<Cell/>");

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]/Grid/Lane[0]", "");
    }

    SECTION ("a child of another type is no cell")
    {
        const auto project = validDocument();
        append (gridLaneOf (project), "<Accent/>");

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 2: the skipMask has exactly stepCount characters, each '0' or '1'", "[model][Validator]")
{
    SECTION ("one too few")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "0001000");

        requireOnlyViolation (project, stepClipGrid, "skipMask");
    }

    SECTION ("one too many")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "000100000");

        requireOnlyViolation (project, stepClipGrid, "skipMask");
    }

    SECTION ("empty")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "");

        requireOnlyViolation (project, stepClipGrid, "skipMask");
    }

    SECTION ("another character")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "000x0000");

        requireOnlyViolation (project, stepClipGrid, "skipMask");
    }
}

TEST_CASE ("Invariant 2: resetAt lies between 1 and stepCount", "[model][Validator]")
{
    SECTION ("the two ends are allowed")
    {
        // A loop of one column is 240 ticks, so the offset of 480 has to go.
        const auto project = validDocument();
        remove (placementOf (project, 1), ids::offset);

        set (gridOf (project), ids::resetAt, "1");
        requireNoViolation (project);

        set (gridOf (project), ids::resetAt, "8");
        requireNoViolation (project);
    }

    SECTION ("0")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::resetAt, "0");

        requireOnlyViolation (project, stepClipGrid, "resetAt");
    }

    SECTION ("past the last column")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::resetAt, "9");

        requireOnlyViolation (project, stepClipGrid, "resetAt");
    }

    SECTION ("not a number")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::resetAt, "six");

        requireOnlyViolation (project, stepClipGrid, "resetAt");
    }
}

TEST_CASE ("Invariant 2: a Grid has a stepCount to measure against", "[model][Validator]")
{
    SECTION ("not a number")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::stepCount, "eight");

        requireOnlyViolation (project, stepClipGrid, "stepCount");
    }

    SECTION ("no column at all")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::stepCount, "0");

        requireOnlyViolation (project, stepClipGrid, "stepCount");
    }
}

TEST_CASE ("Invariant 2: one column before resetAt is left to play", "[model][Validator]")
{
    SECTION ("every column before resetAt skipped")
    {
        // Columns 6 and 7 are not skipped, but they lie past the loop end.
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "11111100");

        requireOnlyViolation (project, stepClipGrid, "");
    }

    SECTION ("one left")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "11110111");
        remove (placementOf (project, 1), ids::offset);

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 2: a step clip has exactly one Grid", "[model][Validator]")
{
    SECTION ("none")
    {
        const auto project = validDocument();
        clipOf (project, 0).removeAllChildren (nullptr);

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]", "");
    }

    SECTION ("two")
    {
        const auto project = validDocument();
        append (clipOf (project, 0), "<Grid/>");

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]", "");
    }
}

//==============================================================================
// Invariant 3: placements within one parent do not overlap.

TEST_CASE ("Invariant 3: a placement may begin where the one before it ends", "[model][Validator]")
{
    // The valid document has exactly that, and a lane placement over the
    // first track placement besides: different parents never overlap.
    requireNoViolation (validDocument());
}

TEST_CASE ("Invariant 3: two placements on one track do not share a tick", "[model][Validator]")
{
    SECTION ("the later one starts one tick early")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::start, "7679");

        requireOnlyViolation (project, secondPlacement, "start");
    }

    SECTION ("document order is not time order")
    {
        // The first placement in the document now starts inside the second.
        const auto project = validDocument();
        set (placementOf (project, 0), ids::start, "10000");

        requireOnlyViolation (project, firstPlacement, "start");
    }

    SECTION ("the same start")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::start, "0");

        requireOnlyViolation (project, secondPlacement, "start");
    }
}

TEST_CASE ("Invariant 3: a placement inside a long one overlaps it, whatever lies between", "[model][Validator]")
{
    // A covers [0, 7680). B [100, 200) and C [300, 400) lie inside A and do
    // not touch each other; both are reported, each against A.
    const auto project = validDocument();
    placementsOf (project).removeChild (1, nullptr);
    append (placementsOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="100" length="100"/>)");
    append (placementsOf (project), R"(<Placement id="0d3f1b2c5e7a4c9b9bac1d2e3f4a5b6c" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="300" length="100"/>)");

    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    REQUIRE (violations.size() == 2);
    CHECK (violations[0].nodePath == secondPlacement);
    CHECK (violations[0].reason.contains (firstPlacement));
    CHECK (violations[1].nodePath == "Project/Tracks/Track[0]/Placements/Placement[2]");
    CHECK (violations[1].reason.contains (firstPlacement));
}

TEST_CASE ("Invariant 3: two placements in one automation lane do not share a tick", "[model][Validator]")
{
    const auto project = validDocument();
    append (automationLaneOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="5759" length="1920"/>)");

    requireOnlyViolation (project, "Project/Tracks/Track[0]/AutomationLanes/AutomationLane[0]/Placement[1]", "start");
}

TEST_CASE ("Invariant 3: placements in two automation lanes of one track may share ticks", "[model][Validator]")
{
    const auto project = validDocument();
    append (trackOf (project).getChildWithName (ids::AutomationLanes),
            R"(<AutomationLane id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b">
                 <Placement id="0d3f1b2c5e7a4c9b9bac1d2e3f4a5b6c" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="3840" length="1920"/>
               </AutomationLane>)");

    requireNoViolation (project);
}

TEST_CASE ("Invariant 3: a placement needs a start and a length to be placed at all", "[model][Validator]")
{
    SECTION ("no start")
    {
        const auto project = validDocument();
        remove (placementOf (project, 1), ids::start);

        requireOnlyViolation (project, secondPlacement, "start");
    }

    SECTION ("no length")
    {
        const auto project = validDocument();
        remove (placementOf (project, 1), ids::length);

        requireOnlyViolation (project, secondPlacement, "length");
    }

    SECTION ("a start that is not a number")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::start, "bar 3");

        requireOnlyViolation (project, secondPlacement, "start");
    }

    SECTION ("a length that is not a whole number")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::length, "7680.5");

        requireOnlyViolation (project, secondPlacement, "length");
    }
}

TEST_CASE ("Invariant 3: a placement at the far end of the tick range is compared without overflow", "[model][Validator]")
{
    // start + length does not fit 64 bits. The sanitizer build would report
    // an overflowing sum.
    const auto project = validDocument();
    set (placementOf (project, 1), ids::start, "9223372036854775000");
    append (placementsOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="9223372036854775800" length="7680"/>)");

    requireOnlyViolation (project, "Project/Tracks/Track[0]/Placements/Placement[2]", "start");
}

//==============================================================================
// Invariant 4: 0 <= offset < loopLength of the referenced clip.

TEST_CASE ("Invariant 4: an offset lies inside the loop of a step clip", "[model][Validator]")
{
    // The loop of the five-step bass is 1200 ticks.
    SECTION ("the last tick of the loop")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::offset, "1199");

        requireNoViolation (project);
    }

    SECTION ("the loop length itself")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::offset, "1200");

        requireOnlyViolation (project, secondPlacement, "offset");
    }

    SECTION ("negative")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::offset, "-1");

        requireOnlyViolation (project, secondPlacement, "offset");
    }

    SECTION ("not a number")
    {
        const auto project = validDocument();
        set (placementOf (project, 1), ids::offset, "half");

        requireOnlyViolation (project, secondPlacement, "offset");
    }
}

TEST_CASE ("Invariant 4: an offset lies inside the loop of an event clip", "[model][Validator]")
{
    SECTION ("the last tick of the loop")
    {
        const auto project = validDocument();
        set (lanePlacementOf (project), ids::offset, "1919");

        requireNoViolation (project);
    }

    SECTION ("the loop length itself")
    {
        const auto project = validDocument();
        set (lanePlacementOf (project), ids::offset, "1920");

        requireOnlyViolation (project, lanePlacement, "offset");
    }
}

TEST_CASE ("Invariant 4: an offset is judged against the loop its clip has now", "[model][Validator]")
{
    // The second placement starts 480 ticks into the loop.
    SECTION ("a loop shortened to three columns still holds it")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::resetAt, "3");

        requireNoViolation (project);
    }

    SECTION ("a loop shortened to two columns does not")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::resetAt, "2");

        requireOnlyViolation (project, secondPlacement, "offset");
    }

    SECTION ("nor does one of five shorter columns")
    {
        const auto project = validDocument();
        set (gridOf (project), ids::stepLength, "96");

        requireOnlyViolation (project, secondPlacement, "offset");
    }

    SECTION ("nor one with three more columns skipped")
    {
        // Columns 0 and 5 are left: 480 ticks.
        const auto project = validDocument();
        set (gridOf (project), ids::skipMask, "01111000");

        requireOnlyViolation (project, secondPlacement, "offset");
    }
}

TEST_CASE ("Invariant 4: a step length too large to multiply is not multiplied", "[model][Validator]")
{
    // Out of its range, which is another invariant's finding; here it must
    // only not overflow. The sanitizer build would report it.
    const auto project = validDocument();
    set (gridOf (project), ids::stepLength, "9223372036854775807");

    requireNoViolation (project);
}

//==============================================================================
// Invariant 5: the kind of a clip.

TEST_CASE ("Invariant 5: a clip has a kind, and it is event or step", "[model][Validator]")
{
    SECTION ("no kind")
    {
        const auto project = validDocument();
        remove (clipOf (project, 1), ids::kind);

        requireOnlyViolation (project, "Project/ClipPool/Clip[1]", "kind");
    }

    SECTION ("another kind")
    {
        const auto project = validDocument();
        set (clipOf (project, 0), ids::kind, "drum");

        requireOnlyViolation (project, "Project/ClipPool/Clip[0]", "kind");
    }

    SECTION ("another case")
    {
        const auto project = validDocument();
        set (clipOf (project, 1), ids::kind, "Event");

        requireOnlyViolation (project, "Project/ClipPool/Clip[1]", "kind");
    }
}

//==============================================================================
// Invariant 6: the Timeline.

TEST_CASE ("Invariant 6: tempo and time signature may change later in the piece", "[model][Validator]")
{
    const auto project = validDocument();
    append (timelineOf (project), R"(<TempoEvent tick="3840" bpm="124"/>)");
    append (timelineOf (project), R"(<TimeSigEvent tick="3840" numerator="3" denominator="4"/>)");

    requireNoViolation (project);
}

TEST_CASE ("Invariant 6: a Timeline has a TempoEvent at tick 0", "[model][Validator]")
{
    SECTION ("none at all")
    {
        const auto project = validDocument();
        timelineOf (project).removeChild (timelineOf (project).getChildWithName (ids::TempoEvent), nullptr);

        requireOnlyViolation (project, "Project/Timeline", "");
    }

    SECTION ("the only one is later")
    {
        const auto project = validDocument();
        set (timelineOf (project).getChildWithName (ids::TempoEvent), ids::tick, "960");

        requireOnlyViolation (project, "Project/Timeline", "");
    }
}

TEST_CASE ("Invariant 6: a Timeline has a TimeSigEvent at tick 0", "[model][Validator]")
{
    SECTION ("none at all")
    {
        const auto project = validDocument();
        timelineOf (project).removeChild (timelineOf (project).getChildWithName (ids::TimeSigEvent), nullptr);

        requireOnlyViolation (project, "Project/Timeline", "");
    }

    SECTION ("the only one is later")
    {
        const auto project = validDocument();
        set (timelineOf (project).getChildWithName (ids::TimeSigEvent), ids::tick, "3840");

        requireOnlyViolation (project, "Project/Timeline", "");
    }
}

TEST_CASE ("Invariant 6: two events of one type at one tick are refused, not reduced to one", "[model][Validator]")
{
    SECTION ("two TempoEvents at tick 0")
    {
        const auto project = validDocument();
        append (timelineOf (project), R"(<TempoEvent tick="0" bpm="124"/>)");

        requireOnlyViolation (project, "Project/Timeline/TempoEvent[1]", "tick");
    }

    SECTION ("two TempoEvents at a later tick")
    {
        const auto project = validDocument();
        append (timelineOf (project), R"(<TempoEvent tick="3840" bpm="124"/>)");
        append (timelineOf (project), R"(<TempoEvent tick="3840" bpm="124"/>)");

        requireOnlyViolation (project, "Project/Timeline/TempoEvent[2]", "tick");
    }

    SECTION ("two TimeSigEvents at tick 0")
    {
        const auto project = validDocument();
        append (timelineOf (project), R"(<TimeSigEvent tick="0" numerator="3" denominator="4"/>)");

        requireOnlyViolation (project, "Project/Timeline/TimeSigEvent[1]", "tick");
    }

    SECTION ("a TempoEvent and a TimeSigEvent at one tick are two types")
    {
        requireNoViolation (validDocument());
    }
}

TEST_CASE ("Invariant 6: a Timeline event has a tick", "[model][Validator]")
{
    SECTION ("none")
    {
        const auto project = validDocument();
        append (timelineOf (project), R"(<TempoEvent bpm="124"/>)");

        requireOnlyViolation (project, "Project/Timeline/TempoEvent[1]", "tick");
    }

    SECTION ("not a number")
    {
        const auto project = validDocument();
        append (timelineOf (project), R"(<TimeSigEvent tick="bar 2" numerator="3" denominator="4"/>)");

        requireOnlyViolation (project, "Project/Timeline/TimeSigEvent[1]", "tick");
    }
}
