// model::commands for a track's automation lanes (docs/SCHEMA.md ->
// AutomationLanes / AutomationLane).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/PlacementCommands.h"

#include <string>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";

juce::String idOfTrack (const CommandFixture& fixture, int position)
{
    return model::TrackModel (fixture.track (position)).getId();
}

juce::String addedLane (CommandFixture& fixture, int trackPosition, const juce::String& name)
{
    const auto result = commands::addAutomationLane (fixture.project, idOfTrack (fixture, trackPosition), name, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

// The names of a track's lanes in document order.
std::string lanesOf (const CommandFixture& fixture, int trackPosition)
{
    std::string text;

    for (const auto& lane : model::TrackModel (fixture.track (trackPosition)).getAutomationLanes())
        text += lane.getName().toStdString() + "|";

    return text;
}

} // namespace

TEST_CASE ("addAutomationLane makes an empty lane behind the track's others, and undo takes it away again", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addAutomationLane (fixture.project, idOfTrack (fixture, 1), "Cutoff", fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto lanes = model::TrackModel (fixture.track (1)).getAutomationLanes();
    REQUIRE (lanes.size() == 1);
    CHECK (lanes[0].getId() == result.createdId);
    CHECK (lanes[0].getName() == "Cutoff");
    CHECK (lanes[0].getPlacements().empty());
    CHECK (lanesOf (fixture, 0).empty());

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::addAutomationLane (fixture.project, idOfTrack (fixture, 1), "Res", fixture.undoManager).wasApplied());
    CHECK (lanesOf (fixture, 1) == "Cutoff|Res|");

    REQUIRE (fixture.undoManager.undo());
    CHECK (lanesOf (fixture, 1) == "Cutoff|");
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("Two lanes that are added get two ids, and may share a name or have none", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;

    const auto first = addedLane (fixture, 0, "");
    const auto second = addedLane (fixture, 0, "");

    CHECK (first != second);
    CHECK (fixture.isValid());
    CHECK (lanesOf (fixture, 0) == "||");
}

TEST_CASE ("addAutomationLane refuses a track that is not there", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    const auto result = commands::addAutomationLane (fixture.project, unknownId, "Cutoff", fixture.undoManager);

    CHECK (! result.wasApplied());
    CHECK (result.createdId.isEmpty());
    CHECK (! commands::addAutomationLane (fixture.project, "", "Cutoff", fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeAutomationLane removes the lane with its placements and leaves their clips in the pool, in one undo step", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto clip = commands::addEventClip (fixture.project, "Sweep", 3840, fixture.undoManager);
    REQUIRE (clip.wasApplied());

    addedLane (fixture, 0, "Cutoff");
    const auto laneId = addedLane (fixture, 0, "Res");
    REQUIRE (commands::addLanePlacement (fixture.project, laneId, clip.createdId, 0, 3840, fixture.undoManager).wasApplied());
    REQUIRE (commands::addLanePlacement (fixture.project, laneId, clip.createdId, 7680, 3840, fixture.undoManager).wasApplied());
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    const model::TracksModel tracks (fixture.project.getChildWithName (ids::Tracks));
    REQUIRE (tracks.getRefcount (clip.createdId) == 2);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeAutomationLane (fixture.project, laneId, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (lanesOf (fixture, 0) == "Cutoff|");
    CHECK (tracks.getRefcount (clip.createdId) == 0);
    CHECK (fixture.project.getChildWithName (ids::ClipPool).getNumChildren() == 1);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
}

TEST_CASE ("moveAutomationLane puts the lane at that position among its track's lanes and keeps the order of the others", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto cutoff = addedLane (fixture, 0, "Cutoff");
    addedLane (fixture, 0, "Res");
    const auto pan = addedLane (fixture, 0, "Pan");
    addedLane (fixture, 1, "Other track");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveAutomationLane (fixture.project, cutoff, 2, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanesOf (fixture, 0) == "Res|Pan|Cutoff|");

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveAutomationLane (fixture.project, pan, 0, fixture.undoManager).wasApplied());
    CHECK (lanesOf (fixture, 0) == "Pan|Res|Cutoff|");
    CHECK (lanesOf (fixture, 1) == "Other track|");

    REQUIRE (fixture.undoManager.undo());
    CHECK (lanesOf (fixture, 0) == "Res|Pan|Cutoff|");
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("moveAutomationLane refuses a position that is not there; the lanes of other tracks do not count", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto cutoff = addedLane (fixture, 0, "Cutoff");
    addedLane (fixture, 0, "Res");
    addedLane (fixture, 1, "Other track");
    addedLane (fixture, 1, "Other track 2");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (! commands::moveAutomationLane (fixture.project, cutoff, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::moveAutomationLane (fixture.project, cutoff, 2, fixture.undoManager).wasApplied());

    // To where it is: applied, and nothing to undo.
    CHECK (commands::moveAutomationLane (fixture.project, cutoff, 0, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setAutomationLaneName names a lane, and undo gives it its old name", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    const auto laneId = addedLane (fixture, 2, "Cutoff");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setAutomationLaneName (fixture.project, laneId, "Filter", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanesOf (fixture, 2) == "Filter|");

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setAutomationLaneName (fixture.project, laneId, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanesOf (fixture, 2) == "|");

    REQUIRE (fixture.undoManager.undo());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A lane command for a lane that is not there is refused and changes nothing", "[model][commands][AutomationLaneCommands]")
{
    CommandFixture fixture;
    addedLane (fixture, 0, "Cutoff");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();
    const auto trackId = idOfTrack (fixture, 0);

    const auto results = {
        commands::removeAutomationLane (fixture.project, unknownId, fixture.undoManager),
        commands::removeAutomationLane (fixture.project, "", fixture.undoManager),
        commands::moveAutomationLane (fixture.project, unknownId, 0, fixture.undoManager),
        commands::setAutomationLaneName (fixture.project, unknownId, "x", fixture.undoManager),
        // The id of the lane's track names no lane.
        commands::removeAutomationLane (fixture.project, trackId, fixture.undoManager),
        commands::setAutomationLaneName (fixture.project, trackId, "x", fixture.undoManager),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}
