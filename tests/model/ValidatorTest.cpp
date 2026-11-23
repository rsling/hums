// model::Validator: the twelve invariants of docs/SCHEMA.md -> Invariants.
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
// filter sweep in a lane over it, and a slot. A third clip that nothing
// references holds notes and the other kinds of event, so that every node
// type of the schema is there at least once.
juce::ValueTree validDocument()
{
    return juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" name="Berlin test" ppq="960">
             <Routing>
               <Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>
               <Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>
               <PadMap>
                 <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
                 <Pad index="1" inputPitch="37" pitch="42"/>
               </PadMap>
             </Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="118"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
               <Marker tick="7680" name="B"/>
             </Timeline>
             <ClipPool>
               <Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" name="Bass 5-step" kind="step">
                 <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000">
                   <Lane id="1e4b7a0d3c6f49528b5e8a1d4c7f0a3b">
                     <Cell pitch="36" on="1"/><Cell pitch="36" on="1"/><Cell pitch="43" on="1"/><Cell pitch="48"/>
                     <Cell pitch="41" on="1" velocity="95" gate="1.2"><CcValue cc="74" value="90"/><CcValue cc="71" value="10"/></Cell>
                     <Cell pitch="39" on="1"/><Cell pitch="36" on="1"/><Cell pitch="36" on="1"/>
                   </Lane>
                 </Grid>
               </Clip>
               <Clip id="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" name="Sweep" kind="event" loopLength="1920">
                 <Events>
                   <Event tick="0" kind="cc" a="74" b="20"/>
                   <Event tick="960" kind="cc" a="74" b="90"/>
                 </Events>
               </Clip>
               <Clip id="a4c6e8b0d2f44a1c8e0b2d4f6a8c0e2b" name="Lead" kind="event" loopLength="3840" quantise="1/8T" colour="#FF336699">
                 <Events>
                   <Note tick="0" length="480" pitch="60"/>
                   <Note tick="470" length="240" pitch="64" velocity="90" offVelocity="0" enabled="0"/>
                   <Event tick="0" kind="cc" a="0" b="1"/>
                   <Event tick="0" kind="programChange" a="5"/>
                   <Event tick="0" kind="pitchBend" a="16383"/>
                   <Event tick="240" kind="channelPressure" a="127"/>
                   <Event tick="240" kind="polyPressure" a="60" b="127" enabled="0"/>
                   <Event tick="960" kind="sysex" data="7e7F0901"/>
                 </Events>
               </Clip>
             </ClipPool>
             <Tracks>
               <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass" channel="16" mute="0" solo="1"
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
                 <Knobs>
                   <Knob cc="74" label="Cutoff"/>
                   <Knob cc="71"/>
                 </Knobs>
                 <Slots>
                   <Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>
                 </Slots>
               </Track>
             </Tracks>
             <Session launchQuantise="0">
               <Scene index="0" name="A"/>
               <Scene index="3"/>
             </Session>
             <UiState activeTab="stepseq" sharedEditPolicy="makeUnique" loopStart="0" loopEnd="7680" loopEnabled="1" exportLength="15360"/>
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

// 0 is the step clip, 1 the automation clip, 2 the clip with notes.
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

juce::ValueTree routingOf (const juce::ValueTree& project)
{
    return project.getChildWithName (ids::Routing);
}

juce::ValueTree padOf (const juce::ValueTree& project, int index)
{
    return routingOf (project).getChildWithName (ids::PadMap).getChild (index);
}

// The Events of the clip with notes: children 0 and 1 are its Notes, 2 to 7
// its Events, in the order cc, programChange, pitchBend, channelPressure,
// polyPressure, sysex.
juce::ValueTree noteClipChildOf (const juce::ValueTree& project, int index)
{
    return clipOf (project, 2).getChildWithName (ids::Events).getChild (index);
}

// The fifth cell of the step clip's lane, the one with CcValues.
juce::ValueTree cellOf (const juce::ValueTree& project)
{
    return gridLaneOf (project).getChild (4);
}

juce::ValueTree sessionOf (const juce::ValueTree& project)
{
    return project.getChildWithName (ids::Session);
}

juce::ValueTree uiStateOf (const juce::ValueTree& project)
{
    return project.getChildWithName (ids::UiState);
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

// How many of the violations are at this node and property.
int countOf (const Violations& violations, const char* nodePath, const char* property)
{
    int count = 0;

    for (const auto& violation : violations)
        if (violation.nodePath.toStdString() == nodePath && violation.property.toStdString() == property)
            ++count;

    return count;
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
const char* const stepClip = "Project/ClipPool/Clip[0]";
const char* const automationClip = "Project/ClipPool/Clip[1]";
const char* const noteClip = "Project/ClipPool/Clip[2]";
const char* const firstNote = "Project/ClipPool/Clip[2]/Events/Note[0]";
const char* const ccEvent = "Project/ClipPool/Clip[2]/Events/Event[0]";
const char* const sysexEvent = "Project/ClipPool/Clip[2]/Events/Event[5]";
const char* const cellWithCcValues = "Project/ClipPool/Clip[0]/Grid/Lane[0]/Cell[4]";
const char* const track = "Project/Tracks/Track[0]";
const char* const uiState = "Project/UiState";

// One property set to a text that breaks it, and the violation that names
// exactly that node and property.
void requireRefused (juce::ValueTree node, const juce::Identifier& property, const char* text, const char* nodePath)
{
    INFO (property.toString() << " = \"" << text << "\"");
    const auto wasThere = node.hasProperty (property);
    const auto before = node[property];

    set (node, property, text);
    requireOnlyViolation (node.getRoot(), nodePath, property.toString().toRawUTF8());

    // Back to what it was, so that one document serves a row of values.
    if (wasThere)
        node.setProperty (property, before, nullptr);
    else
        remove (node, property);
}

void requireAccepted (juce::ValueTree node, const juce::Identifier& property, const char* text)
{
    INFO (property.toString() << " = \"" << text << "\"");
    set (node, property, text);
    requireNoViolation (node.getRoot());
}

void requireRequired (juce::ValueTree node, const juce::Identifier& property, const char* nodePath)
{
    INFO (property.toString() << " removed");
    remove (node, property);
    requireOnlyViolation (node.getRoot(), nodePath, property.toString().toRawUTF8());
}

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
    // an overflowing sum. Invariant 8 refuses each of the two for its end,
    // and the overlap of the two is still found.
    const auto project = validDocument();
    set (placementOf (project, 1), ids::start, "9223372036854775000");
    append (placementsOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="9223372036854775800" length="7680"/>)");

    const char* const thirdPlacement = "Project/Tracks/Track[0]/Placements/Placement[2]";
    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    REQUIRE (violations.size() == 3);
    CHECK (countOf (violations, thirdPlacement, "start") == 1);
    CHECK (countOf (violations, secondPlacement, "length") == 1);
    CHECK (countOf (violations, thirdPlacement, "length") == 1);
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
    // Out of its range, which is invariant 10's finding; here it must only
    // not overflow. The sanitizer build would report it.
    const auto project = validDocument();
    set (gridOf (project), ids::stepLength, "9223372036854775807");

    requireOnlyViolation (project, stepClipGrid, "stepLength");
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

//==============================================================================
// Invariant 7: ppq.

TEST_CASE ("Invariant 7: the ppq is 960, stored or not", "[model][Validator]")
{
    SECTION ("not stored: 960 applies")
    {
        const auto project = validDocument();
        remove (project, ids::ppq);

        requireNoViolation (project);
    }

    SECTION ("another resolution")
    {
        requireRefused (validDocument(), ids::ppq, "480", "Project");
    }

    SECTION ("not a number")
    {
        requireRefused (validDocument(), ids::ppq, "960.0", "Project");
    }
}

//==============================================================================
// Invariant 8: loop lengths, note lengths, no negative tick.

TEST_CASE ("Invariant 8: an event clip has a loopLength of at least one tick", "[model][Validator]")
{
    SECTION ("one tick is enough")
    {
        // No placement references this clip, so no offset is in the way.
        const auto project = validDocument();
        requireAccepted (clipOf (project, 2), ids::loopLength, "1");
    }

    SECTION ("none")
    {
        const auto project = validDocument();
        requireRequired (clipOf (project, 2), ids::loopLength, noteClip);
    }

    SECTION ("zero, negative, not a whole number")
    {
        const auto project = validDocument();
        requireRefused (clipOf (project, 2), ids::loopLength, "0", noteClip);
        requireRefused (clipOf (project, 2), ids::loopLength, "-3840", noteClip);
        requireRefused (clipOf (project, 2), ids::loopLength, "3840.5", noteClip);
    }

    SECTION ("a referenced clip without one is one violation, not one per placement")
    {
        const auto project = validDocument();
        requireRequired (clipOf (project, 1), ids::loopLength, automationClip);
    }
}

TEST_CASE ("Invariant 8: a step clip has neither loopLength nor quantise", "[model][Validator]")
{
    SECTION ("a loopLength, even the one its Grid gives")
    {
        requireRefused (clipOf (validDocument(), 0), ids::loopLength, "1200", stepClip);
    }

    SECTION ("a quantise, even an empty one")
    {
        const auto project = validDocument();
        requireRefused (clipOf (project, 0), ids::quantise, "1/8", stepClip);
        requireRefused (clipOf (project, 0), ids::quantise, "", stepClip);
    }
}

TEST_CASE ("Invariant 8: a Note has a length of at least one tick", "[model][Validator]")
{
    const auto project = validDocument();
    const auto note = noteClipChildOf (project, 0);

    requireAccepted (note, ids::length, "1");
    requireRefused (note, ids::length, "0", firstNote);
    requireRefused (note, ids::length, "-480", firstNote);
    requireRefused (note, ids::length, "long", firstNote);
    requireRequired (note, ids::length, firstNote);
}

TEST_CASE ("Invariant 8: content past the loop of its clip is kept, not refused", "[model][Validator]")
{
    const auto project = validDocument();

    requireAccepted (noteClipChildOf (project, 0), ids::tick, "999999");
    requireAccepted (noteClipChildOf (project, 2), ids::tick, "3840");
}

TEST_CASE ("Invariant 8: no tick is negative, and every tick is there", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("a Note")
    {
        requireRefused (noteClipChildOf (project, 0), ids::tick, "-1", firstNote);
        requireRefused (noteClipChildOf (project, 0), ids::tick, "1.5", firstNote);
        requireRequired (noteClipChildOf (project, 0), ids::tick, firstNote);
    }

    SECTION ("an Event")
    {
        requireRefused (noteClipChildOf (project, 2), ids::tick, "-1", ccEvent);
        requireRequired (noteClipChildOf (project, 2), ids::tick, ccEvent);
    }

    SECTION ("a Marker")
    {
        const auto marker = timelineOf (project).getChildWithName (ids::Marker);

        requireAccepted (marker, ids::tick, "0");
        requireRefused (marker, ids::tick, "-960", "Project/Timeline/Marker[0]");
        requireRequired (marker, ids::tick, "Project/Timeline/Marker[0]");
    }

    SECTION ("a later TempoEvent")
    {
        append (timelineOf (project), R"(<TempoEvent tick="-960" bpm="124"/>)");

        requireOnlyViolation (project, "Project/Timeline/TempoEvent[1]", "tick");
    }

    SECTION ("a later TimeSigEvent")
    {
        append (timelineOf (project), R"(<TimeSigEvent tick="-3840" numerator="3" denominator="4"/>)");

        requireOnlyViolation (project, "Project/Timeline/TimeSigEvent[1]", "tick");
    }

    SECTION ("the start of a Placement")
    {
        requireRefused (placementOf (project, 0), ids::start, "-1", firstPlacement);
    }

    SECTION ("the start of a Placement in an AutomationLane")
    {
        requireRefused (lanePlacementOf (project), ids::start, "-1920", lanePlacement);
    }
}

TEST_CASE ("Invariant 8: a Placement has a length of at least one tick", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (placementOf (project, 1), ids::length, "0", secondPlacement);
    requireRefused (placementOf (project, 1), ids::length, "-7680", secondPlacement);
    requireAccepted (placementOf (project, 1), ids::length, "1");
}

TEST_CASE ("Invariant 8: a Note does not end past the largest tick", "[model][Validator]")
{
    const auto project = validDocument();
    const auto note = noteClipChildOf (project, 0);

    // The largest tick is 9223372036854775807.
    set (note, ids::tick, "9223372036854775000");

    requireAccepted (note, ids::length, "1");
    requireAccepted (note, ids::length, "807");
    requireRefused (note, ids::length, "808", firstNote);
    requireRefused (note, ids::length, "9223372036854775807", firstNote);

    // A length that is refused for itself is reported once, not twice.
    requireRefused (note, ids::length, "0", firstNote);
}

TEST_CASE ("Invariant 8: a Placement does not end past the largest tick", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("on a track")
    {
        const auto placement = placementOf (project, 1);
        set (placement, ids::start, "9223372036854775000");

        requireAccepted (placement, ids::length, "807");
        requireRefused (placement, ids::length, "808", secondPlacement);
        requireRefused (placement, ids::length, "9223372036854775807", secondPlacement);
        requireRefused (placement, ids::length, "0", secondPlacement);
    }

    SECTION ("in an AutomationLane")
    {
        const auto placement = lanePlacementOf (project);
        set (placement, ids::start, "9223372036854775000");

        requireAccepted (placement, ids::length, "807");
        requireRefused (placement, ids::length, "808", lanePlacement);
    }

    SECTION ("a start that is refused for itself is the only violation")
    {
        requireRefused (placementOf (project, 1), ids::start, "-1", secondPlacement);
    }
}

//==============================================================================
// Invariant 9: what an AutomationLane may reference.

TEST_CASE ("Invariant 9: a clip of cc events may sit in an AutomationLane, disabled events and all", "[model][Validator]")
{
    const auto project = validDocument();
    append (clipOf (project, 1).getChildWithName (ids::Events), R"(<Event tick="480" kind="cc" a="71" b="5" enabled="0"/>)");

    requireNoViolation (project);
}

TEST_CASE ("Invariant 9: a clip in an AutomationLane holds no Note", "[model][Validator]")
{
    SECTION ("an enabled one")
    {
        const auto project = validDocument();
        append (clipOf (project, 1).getChildWithName (ids::Events), R"(<Note tick="0" length="240" pitch="60"/>)");

        requireOnlyViolation (project, "Project/ClipPool/Clip[1]/Events/Note[0]", "");
    }

    SECTION ("a disabled one")
    {
        const auto project = validDocument();
        append (clipOf (project, 1).getChildWithName (ids::Events), R"(<Note tick="0" length="240" pitch="60" enabled="0"/>)");

        requireOnlyViolation (project, "Project/ClipPool/Clip[1]/Events/Note[0]", "");
    }
}

TEST_CASE ("Invariant 9: a clip in an AutomationLane holds no event but cc", "[model][Validator]")
{
    const auto kind = GENERATE ("pitchBend", "programChange", "channelPressure", "polyPressure", "sysex");

    const auto project = validDocument();
    append (clipOf (project, 1).getChildWithName (ids::Events), R"(<Event tick="0" kind="cc"/>)");
    set (clipOf (project, 1).getChildWithName (ids::Events).getChild (2), ids::kind, kind);

    requireOnlyViolation (project, "Project/ClipPool/Clip[1]/Events/Event[2]", "kind");
}

TEST_CASE ("Invariant 9: an AutomationLane takes no step clip", "[model][Validator]")
{
    // The step clip's loop is 1200 ticks; the placement's offset of 0 fits.
    const auto project = validDocument();
    set (lanePlacementOf (project), ids::clipId, "5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1");

    requireOnlyViolation (project, lanePlacement, "clipId");
}

TEST_CASE ("Invariant 9: a clip that two lane placements reference is reported once", "[model][Validator]")
{
    const auto project = validDocument();
    append (automationLaneOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="7680" length="1920"/>)");
    append (clipOf (project, 1).getChildWithName (ids::Events), R"(<Note tick="0" length="240" pitch="60"/>)");

    requireOnlyViolation (project, "Project/ClipPool/Clip[1]/Events/Note[0]", "");
}

TEST_CASE ("Invariant 9: the same content on a Track's own Placements is fine", "[model][Validator]")
{
    // The clip with notes, placed after the two bass placements.
    const auto project = validDocument();
    append (placementsOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="a4c6e8b0d2f44a1c8e0b2d4f6a8c0e2b" start="15360" length="3840"/>)");

    requireNoViolation (project);
}

TEST_CASE ("Invariant 9: a clip that an AutomationLane references is on no Track's own Placements", "[model][Validator]")
{
    // The lane's clip, placed after the two bass placements.
    const auto project = validDocument();
    append (placementsOf (project), R"(<Placement id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="15360" length="3840"/>)");

    requireOnlyViolation (project, "Project/Tracks/Track[0]/Placements/Placement[2]", "clipId");
}

TEST_CASE ("Invariant 9: a clip that an AutomationLane references is in no Slot", "[model][Validator]")
{
    const auto project = validDocument();
    set (slotOf (project), ids::clipId, "6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2");

    requireOnlyViolation (project, "Project/Tracks/Track[0]/Slots/Slot[0]", "clipId");
}

//==============================================================================
// Invariant 10: ranges, lists and text forms.

TEST_CASE ("Invariant 10: pitches, velocities and controller values are 7-bit", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("a Note")
    {
        const auto note = noteClipChildOf (project, 0);

        requireAccepted (note, ids::pitch, "0");
        requireAccepted (note, ids::pitch, "127");
        requireRefused (note, ids::pitch, "128", firstNote);
        requireRefused (note, ids::pitch, "-1", firstNote);
        requireRefused (note, ids::pitch, "C3", firstNote);
        requireAccepted (note, ids::velocity, "1");
        requireAccepted (note, ids::velocity, "127");
        requireRefused (note, ids::velocity, "0", firstNote);
        requireRefused (note, ids::velocity, "128", firstNote);
        requireAccepted (note, ids::offVelocity, "127");
        requireRefused (note, ids::offVelocity, "-1", firstNote);
        requireRefused (note, ids::offVelocity, "128", firstNote);
        requireRefused (note, ids::enabled, "yes", firstNote);
        requireRequired (note, ids::pitch, firstNote);
    }

    SECTION ("a Cell")
    {
        const auto cell = cellOf (project);

        requireAccepted (cell, ids::pitch, "0");
        requireAccepted (cell, ids::pitch, "127");
        requireRefused (cell, ids::pitch, "128", cellWithCcValues);
        requireRefused (cell, ids::velocity, "0", cellWithCcValues);
        requireRefused (cell, ids::velocity, "128", cellWithCcValues);
        requireRefused (cell, ids::on, "true", cellWithCcValues);
    }

    SECTION ("a CcValue")
    {
        const auto ccValue = cellOf (project).getChild (0);
        const auto* path = "Project/ClipPool/Clip[0]/Grid/Lane[0]/Cell[4]/CcValue[0]";

        requireAccepted (ccValue, ids::cc, "0");
        requireAccepted (ccValue, ids::cc, "127");
        requireRefused (ccValue, ids::cc, "128", path);
        requireRefused (ccValue, ids::value, "-1", path);
        requireRefused (ccValue, ids::value, "128", path);
        requireRequired (ccValue, ids::value, path);
    }

    SECTION ("a CcValue without its controller number")
    {
        requireRequired (cellOf (project).getChild (0), ids::cc, "Project/ClipPool/Clip[0]/Grid/Lane[0]/Cell[4]/CcValue[0]");
    }

    SECTION ("the fixedPitch of a Lane")
    {
        const auto lane = gridLaneOf (project);
        const auto* path = "Project/ClipPool/Clip[0]/Grid/Lane[0]";

        requireAccepted (lane, ids::fixedPitch, "0");
        requireAccepted (lane, ids::fixedPitch, "127");
        requireRefused (lane, ids::fixedPitch, "128", path);
        requireRefused (lane, ids::fixedPitch, "", path);
    }

    SECTION ("a Pad")
    {
        const auto pad = padOf (project, 1);
        const auto* path = "Project/Routing/PadMap/Pad[1]";

        requireAccepted (pad, ids::inputPitch, "127");
        requireAccepted (pad, ids::pitch, "0");
        requireRefused (pad, ids::inputPitch, "128", path);
        requireRefused (pad, ids::pitch, "-1", path);
        requireRefused (pad, ids::index, "-1", path);
        requireRefused (pad, ids::index, "one", path);
        requireRequired (pad, ids::pitch, path);
    }

    SECTION ("a Pad without its index")
    {
        requireRequired (padOf (project, 0), ids::index, "Project/Routing/PadMap/Pad[0]");
    }

    SECTION ("a Pad without its inputPitch")
    {
        requireRequired (padOf (project, 1), ids::inputPitch, "Project/Routing/PadMap/Pad[1]");
    }

    SECTION ("a Knob")
    {
        const auto knob = trackOf (project).getChildWithName (ids::Knobs).getChild (1);
        const auto* path = "Project/Tracks/Track[0]/Knobs/Knob[1]";

        requireAccepted (knob, ids::cc, "0");
        requireAccepted (knob, ids::cc, "127");
        requireRefused (knob, ids::cc, "128", path);
        requireRequired (knob, ids::cc, path);
    }
}

TEST_CASE ("Invariant 10: an Event's a and b follow its kind", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("cc: controller and value")
    {
        const auto event = noteClipChildOf (project, 2);

        requireAccepted (event, ids::a, "127");
        requireAccepted (event, ids::b, "0");
        requireRefused (event, ids::a, "128", ccEvent);
        requireRefused (event, ids::b, "128", ccEvent);
        requireRefused (event, ids::b, "-1", ccEvent);
        requireRefused (event, ids::a, "x", ccEvent);
        requireRefused (event, ids::enabled, "2", ccEvent);
    }

    SECTION ("cc without a and b: both are 0")
    {
        remove (noteClipChildOf (project, 2), ids::a);
        remove (noteClipChildOf (project, 2), ids::b);

        requireNoViolation (project);
    }

    SECTION ("programChange: program")
    {
        const auto* path = "Project/ClipPool/Clip[2]/Events/Event[1]";

        requireAccepted (noteClipChildOf (project, 3), ids::a, "127");
        requireRefused (noteClipChildOf (project, 3), ids::a, "128", path);
    }

    SECTION ("pitchBend: 14 bits")
    {
        const auto* path = "Project/ClipPool/Clip[2]/Events/Event[2]";

        requireAccepted (noteClipChildOf (project, 4), ids::a, "0");
        requireAccepted (noteClipChildOf (project, 4), ids::a, "8192");
        requireRefused (noteClipChildOf (project, 4), ids::a, "16384", path);
        requireRefused (noteClipChildOf (project, 4), ids::a, "-1", path);
    }

    SECTION ("channelPressure: pressure")
    {
        const auto* path = "Project/ClipPool/Clip[2]/Events/Event[3]";

        requireAccepted (noteClipChildOf (project, 5), ids::a, "0");
        requireRefused (noteClipChildOf (project, 5), ids::a, "128", path);
    }

    SECTION ("polyPressure: pitch and pressure")
    {
        const auto* path = "Project/ClipPool/Clip[2]/Events/Event[4]";

        requireRefused (noteClipChildOf (project, 6), ids::a, "128", path);
        requireRefused (noteClipChildOf (project, 6), ids::b, "128", path);
    }

    SECTION ("a kind is required and is one of the six")
    {
        requireRefused (noteClipChildOf (project, 2), ids::kind, "noteOn", ccEvent);
        requireRefused (noteClipChildOf (project, 2), ids::kind, "CC", ccEvent);
        requireRequired (noteClipChildOf (project, 2), ids::kind, ccEvent);
    }
}

TEST_CASE ("Invariant 10: a sysex payload is hexadecimal, of even length and at most 64 KB", "[model][Validator]")
{
    const auto project = validDocument();
    const auto event = noteClipChildOf (project, 7);

    SECTION ("an empty payload, and none")
    {
        requireAccepted (event, ids::data, "");
        remove (event, ids::data);
        requireNoViolation (project);
    }

    SECTION ("not hexadecimal")
    {
        requireRefused (event, ids::data, "7e7g", sysexEvent);
        requireRefused (event, ids::data, "7e 7f", sysexEvent);
    }

    SECTION ("half a byte")
    {
        requireRefused (event, ids::data, "7e7", sysexEvent);
    }

    SECTION ("exactly 64 KB")
    {
        set (event, ids::data, juce::String::repeatedString ("00", 65536));
        requireNoViolation (project);
    }

    SECTION ("one byte more")
    {
        set (event, ids::data, juce::String::repeatedString ("00", 65537));
        requireOnlyViolation (project, sysexEvent, "data");
    }
}

TEST_CASE ("Invariant 10: a gate is greater than 0 and at most 4", "[model][Validator]")
{
    const auto project = validDocument();
    const auto cell = cellOf (project);

    requireAccepted (cell, ids::gate, "4");
    requireAccepted (cell, ids::gate, "0.001");
    requireAccepted (cell, ids::gate, "1e-05");
    requireRefused (cell, ids::gate, "0", cellWithCcValues);
    requireRefused (cell, ids::gate, "-0.5", cellWithCcValues);
    requireRefused (cell, ids::gate, "4.00001", cellWithCcValues);
    requireRefused (cell, ids::gate, "inf", cellWithCcValues);
    requireRefused (cell, ids::gate, "nan", cellWithCcValues);
    requireRefused (cell, ids::gate, "half", cellWithCcValues);
}

TEST_CASE ("Invariant 10: a bpm is there, finite and greater than 0", "[model][Validator]")
{
    const auto project = validDocument();
    const auto tempoEvent = timelineOf (project).getChildWithName (ids::TempoEvent);
    const auto* path = "Project/Timeline/TempoEvent[0]";

    requireAccepted (tempoEvent, ids::bpm, "0.001");
    requireAccepted (tempoEvent, ids::bpm, "999.999");
    requireRefused (tempoEvent, ids::bpm, "0", path);
    requireRefused (tempoEvent, ids::bpm, "-118", path);
    requireRefused (tempoEvent, ids::bpm, "1e999", path);
    // 2^32 + 1, which the int JUCE's parser sums an exponent in takes for 1.
    requireRefused (tempoEvent, ids::bpm, "1e4294967297", path);
    requireRefused (tempoEvent, ids::bpm, "1e0001", path);
    requireRefused (tempoEvent, ids::bpm, "inf", path);
    requireRefused (tempoEvent, ids::bpm, "fast", path);
    requireRequired (tempoEvent, ids::bpm, path);
}

TEST_CASE ("Invariant 10: a double is the value its saved text reads back as", "[model][Validator]")
{
    // Six significant digits [D14]; more are refused, not rounded [D17].
    const auto project = validDocument();
    auto tempoEvent = timelineOf (project).getChildWithName (ids::TempoEvent);
    auto cell = cellOf (project);

    SECTION ("the test is on the value, not the spelling")
    {
        requireAccepted (tempoEvent, ids::bpm, "118.0");
        requireAccepted (tempoEvent, ids::bpm, "118.123");
        requireAccepted (tempoEvent, ids::bpm, "118.123000");
        requireAccepted (tempoEvent, ids::bpm, "1.18123e2");
        requireAccepted (cell, ids::gate, "0.333333");
        requireAccepted (cell, ids::gate, "0.50");
    }

    SECTION ("a seventh digit")
    {
        requireRefused (tempoEvent, ids::bpm, "118.1234567", "Project/Timeline/TempoEvent[0]");
        requireRefused (tempoEvent, ids::bpm, "118.1235", "Project/Timeline/TempoEvent[0]");
        requireRefused (tempoEvent, ids::bpm, "1000.123", "Project/Timeline/TempoEvent[0]");
        requireRefused (cell, ids::gate, "0.3333333", cellWithCcValues);
    }

    SECTION ("a double set in this session, with all the digits a double has")
    {
        cell.setProperty (ids::gate, 1.0 / 3.0, nullptr);

        requireOnlyViolation (project, cellWithCcValues, "gate");
    }

    SECTION ("a double set in this session that a save keeps")
    {
        cell.setProperty (ids::gate, 0.75, nullptr);
        tempoEvent.setProperty (ids::bpm, 124.5, nullptr);

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 10: a time signature is 1 to 32 over a power of two up to 64", "[model][Validator]")
{
    const auto project = validDocument();
    const auto timeSigEvent = timelineOf (project).getChildWithName (ids::TimeSigEvent);
    const auto* path = "Project/Timeline/TimeSigEvent[0]";

    requireAccepted (timeSigEvent, ids::numerator, "1");
    requireAccepted (timeSigEvent, ids::numerator, "32");
    requireRefused (timeSigEvent, ids::numerator, "0", path);
    requireRefused (timeSigEvent, ids::numerator, "33", path);
    requireAccepted (timeSigEvent, ids::denominator, "1");
    requireAccepted (timeSigEvent, ids::denominator, "64");
    requireRefused (timeSigEvent, ids::denominator, "0", path);
    requireRefused (timeSigEvent, ids::denominator, "3", path);
    requireRefused (timeSigEvent, ids::denominator, "128", path);
    requireRefused (timeSigEvent, ids::denominator, "-4", path);
    requireRequired (timeSigEvent, ids::denominator, path);
}

TEST_CASE ("Invariant 10: a time signature has a numerator", "[model][Validator]")
{
    const auto project = validDocument();

    requireRequired (timelineOf (project).getChildWithName (ids::TimeSigEvent), ids::numerator, "Project/Timeline/TimeSigEvent[0]");
}

TEST_CASE ("Invariant 10: a quantise is one of the seven listed values", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("the seven")
    {
        const auto quantise = GENERATE ("", "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T");

        requireAccepted (clipOf (project, 2), ids::quantise, quantise);
    }

    SECTION ("not stored: off")
    {
        remove (clipOf (project, 2), ids::quantise);

        requireNoViolation (project);
    }

    SECTION ("anything else")
    {
        requireRefused (clipOf (project, 2), ids::quantise, "1/16", noteClip);
        requireRefused (clipOf (project, 2), ids::quantise, "1/8t", noteClip);
        requireRefused (clipOf (project, 2), ids::quantise, "480", noteClip);
    }
}

TEST_CASE ("Invariant 10: a channel is 1 to 16, a channel filter 0 to 16", "[model][Validator]")
{
    const auto project = validDocument();
    const auto performer = routingOf (project).getChildWithName (ids::Performer);

    requireAccepted (trackOf (project), ids::channel, "1");
    requireRefused (trackOf (project), ids::channel, "0", track);
    requireRefused (trackOf (project), ids::channel, "17", track);
    requireRefused (trackOf (project), ids::channel, "one", track);
    requireAccepted (performer, ids::channelFilter, "0");
    requireAccepted (performer, ids::channelFilter, "16");
    requireRefused (performer, ids::channelFilter, "-1", "Project/Routing/Performer[0]");
    requireRefused (performer, ids::channelFilter, "17", "Project/Routing/Performer[0]");
}

TEST_CASE ("Invariant 10: a bool is 0 or 1 wherever it stands", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (trackOf (project), ids::mute, "true", track);
    requireRefused (trackOf (project), ids::solo, "", track);
    requireRefused (routingOf (project).getChildWithName (ids::Player), ids::sendClock, "on", "Project/Routing/Player[0]");
    requireRefused (uiStateOf (project), ids::loopEnabled, "yes", uiState);
}

TEST_CASE ("Invariant 10: a Performer and a Player have a name", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("an empty one is none")
    {
        requireRefused (routingOf (project).getChildWithName (ids::Player), ids::name, "", "Project/Routing/Player[0]");
        requireRefused (routingOf (project).getChildWithName (ids::Performer), ids::name, "", "Project/Routing/Performer[0]");
    }

    SECTION ("any other text is one")
    {
        requireAccepted (routingOf (project).getChildWithName (ids::Player), ids::name, " ");
        requireAccepted (routingOf (project).getChildWithName (ids::Performer), ids::name, "Minimoog");
    }

    SECTION ("a Performer without")
    {
        requireRequired (routingOf (project).getChildWithName (ids::Performer), ids::name, "Project/Routing/Performer[0]");
    }

    SECTION ("a Player without")
    {
        requireRequired (routingOf (project).getChildWithName (ids::Player), ids::name, "Project/Routing/Player[0]");
    }
}

TEST_CASE ("Invariant 10: a colour is '#' and eight hexadecimal digits, or empty", "[model][Validator]")
{
    const auto project = validDocument();

    requireAccepted (trackOf (project), ids::colour, "");
    requireAccepted (trackOf (project), ids::colour, "#ff00AA10");
    requireRefused (trackOf (project), ids::colour, "red", track);
    requireRefused (trackOf (project), ids::colour, "#336699", track);
    requireRefused (trackOf (project), ids::colour, "FF336699", track);
    requireRefused (clipOf (project, 2), ids::colour, "#FF33669G", noteClip);
    requireRefused (clipOf (project, 0), ids::colour, "#FF3366990", stepClip);
}

TEST_CASE ("Invariant 10: a Grid has 1 to 64 columns of 1 to 3840 ticks", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("the step length")
    {
        // The loop is five columns long; at 96 ticks and more the second
        // placement's offset of 480 still lies inside it.
        requireAccepted (gridOf (project), ids::stepLength, "3840");
        requireAccepted (gridOf (project), ids::stepLength, "97");
        requireRefused (gridOf (project), ids::stepLength, "3841", stepClipGrid);
        requireRefused (gridOf (project), ids::stepLength, "sixteenth", stepClipGrid);
    }

    SECTION ("a step length of 1")
    {
        // The loop is then five ticks long.
        remove (placementOf (project, 1), ids::offset);

        requireAccepted (gridOf (project), ids::stepLength, "1");
        requireRefused (gridOf (project), ids::stepLength, "0", stepClipGrid);
        requireRefused (gridOf (project), ids::stepLength, "-240", stepClipGrid);
    }

    SECTION ("65 columns")
    {
        const auto grid = gridOf (project);
        set (grid, ids::stepCount, "65");
        remove (grid, ids::skipMask);

        for (int cell = 8; cell < 65; ++cell)
            append (gridLaneOf (project), "<Cell/>");

        requireOnlyViolation (project, stepClipGrid, "stepCount");
    }

    SECTION ("64 columns")
    {
        const auto grid = gridOf (project);
        set (grid, ids::stepCount, "64");
        remove (grid, ids::skipMask);

        for (int cell = 8; cell < 64; ++cell)
            append (gridLaneOf (project), "<Cell/>");

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 10: a Slot's scene, a Scene's index and the launch grid are not negative", "[model][Validator]")
{
    const auto project = validDocument();
    const auto scene = sessionOf (project).getChild (1);
    const auto* slotPath = "Project/Tracks/Track[0]/Slots/Slot[0]";
    const auto* scenePath = "Project/Session/Scene[1]";

    SECTION ("a Slot's scene")
    {
        requireRefused (slotOf (project), ids::scene, "-1", slotPath);
        requireRefused (slotOf (project), ids::scene, "A", slotPath);
        requireRequired (slotOf (project), ids::scene, slotPath);
    }

    SECTION ("a Scene's index")
    {
        requireRefused (scene, ids::index, "-1", scenePath);
        requireRequired (scene, ids::index, scenePath);
    }

    SECTION ("the launch grid")
    {
        requireAccepted (sessionOf (project), ids::launchQuantise, "3840");
        requireRefused (sessionOf (project), ids::launchQuantise, "-1", "Project/Session");
        requireRefused (sessionOf (project), ids::launchQuantise, "bar", "Project/Session");
    }
}

TEST_CASE ("Invariant 10: a Project has a schemaVersion", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (project, ids::schemaVersion, "0", "Project");
    requireRefused (project, ids::schemaVersion, "1.0", "Project");
    requireRequired (project, ids::schemaVersion, "Project");
}

TEST_CASE ("Invariant 10: UiState with nothing stored is valid", "[model][Validator]")
{
    const auto project = validDocument();
    uiStateOf (project).removeAllProperties (nullptr);

    requireNoViolation (project);
}

TEST_CASE ("Invariant 10: the active tab is one of the six", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("the six")
    {
        const auto tab = GENERATE ("routing", "pianoroll", "arrangement", "drums", "stepseq", "automation");

        requireAccepted (uiStateOf (project), ids::activeTab, tab);
    }

    SECTION ("anything else")
    {
        requireRefused (uiStateOf (project), ids::activeTab, "mixer", uiState);
        requireRefused (uiStateOf (project), ids::activeTab, "StepSeq", uiState);
        requireRefused (uiStateOf (project), ids::activeTab, "", uiState);
    }
}

TEST_CASE ("Invariant 10: the shared edit policy is updateAll or makeUnique", "[model][Validator]")
{
    const auto project = validDocument();

    requireAccepted (uiStateOf (project), ids::sharedEditPolicy, "updateAll");
    requireRefused (uiStateOf (project), ids::sharedEditPolicy, "ask", uiState);
    requireRefused (uiStateOf (project), ids::sharedEditPolicy, "", uiState);
}

TEST_CASE ("Invariant 10: loop points and export length are not negative", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (uiStateOf (project), ids::loopStart, "-1", uiState);
    requireRefused (uiStateOf (project), ids::loopStart, "bar 1", uiState);
    requireRefused (uiStateOf (project), ids::exportLength, "-1", uiState);
    requireRefused (uiStateOf (project), ids::exportLength, "all", uiState);
    requireAccepted (uiStateOf (project), ids::exportLength, "0");

    // A disabled loop, so that only the sign is judged.
    set (uiStateOf (project), ids::loopEnabled, "0");
    requireRefused (uiStateOf (project), ids::loopEnd, "-1", uiState);
}

TEST_CASE ("Invariant 10: an enabled loop starts before it ends", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("start equal to end")
    {
        set (uiStateOf (project), ids::loopStart, "7680");

        requireOnlyViolation (project, uiState, "loopEnabled");
    }

    SECTION ("start after end")
    {
        set (uiStateOf (project), ids::loopStart, "9000");

        requireOnlyViolation (project, uiState, "loopEnabled");
    }

    SECTION ("enabled with nothing else stored: 0 to 0")
    {
        remove (uiStateOf (project), ids::loopStart);
        remove (uiStateOf (project), ids::loopEnd);

        requireOnlyViolation (project, uiState, "loopEnabled");
    }

    SECTION ("the same points on a disabled loop are fine")
    {
        set (uiStateOf (project), ids::loopStart, "9000");
        set (uiStateOf (project), ids::loopEnabled, "0");

        requireNoViolation (project);
    }

    SECTION ("a malformed end is one violation, not two")
    {
        set (uiStateOf (project), ids::loopEnd, "end");

        requireOnlyViolation (project, uiState, "loopEnd");
    }
}

TEST_CASE ("Invariant 10: a view's own node under UiState is not looked into", "[model][Validator]")
{
    const auto project = validDocument();
    append (uiStateOf (project), R"(<StepSeqState zoom="wide" loopEnabled="perhaps"/>)");

    requireNoViolation (project);
}

//==============================================================================
// Invariant 11: unique keys.

TEST_CASE ("Invariant 11: a pad index and an input pitch occur once in the PadMap", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("two pads may produce the same pitch")
    {
        requireAccepted (padOf (project, 1), ids::pitch, "36");
    }

    SECTION ("the same index")
    {
        requireRefused (padOf (project, 1), ids::index, "0", "Project/Routing/PadMap/Pad[1]");
    }

    SECTION ("the same input pitch")
    {
        requireRefused (padOf (project, 1), ids::inputPitch, "36", "Project/Routing/PadMap/Pad[1]");
    }
}

TEST_CASE ("Invariant 11: a Scene index occurs once", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (sessionOf (project).getChild (1), ids::index, "0", "Project/Session/Scene[1]");
}

TEST_CASE ("Invariant 11: a scene has one Slot per track", "[model][Validator]")
{
    const auto project = validDocument();
    const auto* secondSlot = R"(<Slot scene="1" clipId="a4c6e8b0d2f44a1c8e0b2d4f6a8c0e2b"/>)";

    SECTION ("another scene on the same track")
    {
        append (trackOf (project).getChildWithName (ids::Slots), secondSlot);

        requireNoViolation (project);
    }

    SECTION ("the same scene on the same track")
    {
        append (trackOf (project).getChildWithName (ids::Slots), secondSlot);
        set (trackOf (project).getChildWithName (ids::Slots).getChild (1), ids::scene, "0");

        requireOnlyViolation (project, "Project/Tracks/Track[0]/Slots/Slot[1]", "scene");
    }

    SECTION ("the same scene on another track")
    {
        append (project.getChildWithName (ids::Tracks),
                R"(<Track id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" name="Drums">
                     <Placements/>
                     <AutomationLanes/>
                     <Knobs><Knob cc="74"/></Knobs>
                     <Slots><Slot scene="0" clipId="a4c6e8b0d2f44a1c8e0b2d4f6a8c0e2b"/></Slots>
                   </Track>)");

        requireNoViolation (project);
    }
}

TEST_CASE ("Invariant 11: a controller number has one Knob per track", "[model][Validator]")
{
    const auto project = validDocument();

    requireRefused (trackOf (project).getChildWithName (ids::Knobs).getChild (1), ids::cc, "74", "Project/Tracks/Track[0]/Knobs/Knob[1]");
}

TEST_CASE ("Invariant 11: a controller number has one CcValue per cell", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("the same number in another cell")
    {
        append (gridLaneOf (project).getChild (0), R"(<CcValue cc="74" value="1"/>)");

        requireNoViolation (project);
    }

    SECTION ("the same number in the same cell")
    {
        requireRefused (cellOf (project).getChild (1), ids::cc, "74", "Project/ClipPool/Clip[0]/Grid/Lane[0]/Cell[4]/CcValue[1]");
    }
}

TEST_CASE ("Invariant 11: no two Players share a name, and no two Performers", "[model][Validator]")
{
    const auto project = validDocument();

    SECTION ("two Players")
    {
        append (routingOf (project), R"(<Player id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" name="Minimoog"/>)");

        requireOnlyViolation (project, "Project/Routing/Player[1]", "name");
    }

    SECTION ("two Performers")
    {
        append (routingOf (project), R"(<Performer id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" name="Keyboard"/>)");

        requireOnlyViolation (project, "Project/Routing/Performer[1]", "name");
    }

    SECTION ("a Player and a Performer may")
    {
        append (routingOf (project), R"(<Performer id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" name="Minimoog"/>)");
        append (routingOf (project), R"(<Player id="8d3f0b1c5e704c9b9bac1d2e3f405b6c" name="Keyboard"/>)");

        requireNoViolation (project);
    }

    SECTION ("another case is another name")
    {
        append (routingOf (project), R"(<Player id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b" name="minimoog"/>)");

        requireNoViolation (project);
    }
}

//==============================================================================
// Invariant 12: containers.

TEST_CASE ("Invariant 12: a Project has each of its six containers", "[model][Validator]")
{
    // Routing, Session and UiState can go without taking anything else
    // along; the other three are in the next test.
    const auto container = GENERATE ("Session", "UiState");

    auto project = validDocument();
    project.removeChild (project.getChildWithName (container), nullptr);

    requireOnlyViolation (project, "Project", "");
}

TEST_CASE ("Invariant 12: an empty Project lacks all six containers", "[model][Validator]")
{
    const auto project = juce::ValueTree::fromXml (R"(<Project schemaVersion="1"/>)");
    const auto violations = model::Validator::validate (project);

    INFO (describe (violations));
    REQUIRE (violations.size() == 6);

    for (const auto& violation : violations)
    {
        CHECK (violation.nodePath == "Project");
        CHECK (violation.property.isEmpty());
    }
}

TEST_CASE ("Invariant 12: the smallest valid Project", "[model][Validator]")
{
    requireNoViolation (juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1">
             <Routing><PadMap/></Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="120"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
             </Timeline>
             <ClipPool/>
             <Tracks/>
             <Session/>
             <UiState/>
           </Project>)"));
}

TEST_CASE ("Invariant 12: a Project has each container once", "[model][Validator]")
{
    const auto container = GENERATE ("<Routing><PadMap/></Routing>", "<ClipPool/>", "<Tracks/>", "<Session/>", "<UiState/>");

    const auto project = validDocument();
    append (project, container);

    requireOnlyViolation (project, "Project", "");
}

TEST_CASE ("Invariant 12: a second Timeline is refused, and judged like the first", "[model][Validator]")
{
    const auto project = validDocument();
    append (project, R"(<Timeline><TempoEvent tick="0" bpm="120"/><TimeSigEvent tick="0" numerator="4" denominator="4"/></Timeline>)");

    requireOnlyViolation (project, "Project", "");
}

TEST_CASE ("Invariant 12: a Track has each of its four containers, once", "[model][Validator]")
{
    SECTION ("one missing")
    {
        // An empty one of each: taking Placements or Slots away would take
        // the references to the clips along, which is no violation.
        const auto container = GENERATE ("<Placements/>", "<AutomationLanes/>", "<Knobs/>", "<Slots/>");

        const auto project = validDocument();
        append (project.getChildWithName (ids::Tracks),
                R"(<Track id="7c2e9a0b4d6f4b8a8a9b0c1d2e3f4a5b"><Placements/><AutomationLanes/><Knobs/><Slots/></Track>)");

        auto secondTrack = project.getChildWithName (ids::Tracks).getChild (1);
        requireNoViolation (project);

        secondTrack.removeChild (secondTrack.getChildWithName (juce::ValueTree::fromXml (container).getType()), nullptr);

        requireOnlyViolation (project, "Project/Tracks/Track[1]", "");
    }

    SECTION ("one twice")
    {
        const auto container = GENERATE ("<Placements/>", "<AutomationLanes/>", "<Knobs/>", "<Slots/>");

        const auto project = validDocument();
        append (trackOf (project), container);

        requireOnlyViolation (project, track, "");
    }
}

TEST_CASE ("Invariant 12: Routing has its PadMap, once", "[model][Validator]")
{
    SECTION ("none")
    {
        const auto project = validDocument();
        routingOf (project).removeChild (routingOf (project).getChildWithName (ids::PadMap), nullptr);

        requireOnlyViolation (project, "Project/Routing", "");
    }

    SECTION ("two")
    {
        const auto project = validDocument();
        append (routingOf (project), "<PadMap/>");

        requireOnlyViolation (project, "Project/Routing", "");
    }
}

TEST_CASE ("Invariant 12: an event clip has its Events, once", "[model][Validator]")
{
    SECTION ("empty Events are enough")
    {
        const auto project = validDocument();
        clipOf (project, 2).getChildWithName (ids::Events).removeAllChildren (nullptr);

        requireNoViolation (project);
    }

    SECTION ("none")
    {
        const auto project = validDocument();
        clipOf (project, 2).removeChild (clipOf (project, 2).getChildWithName (ids::Events), nullptr);

        requireOnlyViolation (project, noteClip, "");
    }

    SECTION ("two")
    {
        const auto project = validDocument();
        append (clipOf (project, 2), "<Events/>");

        requireOnlyViolation (project, noteClip, "");
    }
}
