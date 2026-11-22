// model::commands for what a step clip holds: lanes, cells, CC values and the
// grid's step length, step count, reset and skipped columns (docs/SCHEMA.md
// -> Step clip: Grid, -> Loop-length changes; docs/EDITING.md).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/ClipPoolModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/SlotCommands.h"
#include "model/commands/StepClipCommands.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;
using commands::CellValues;
using commands::CommandResult;
using commands::EditContext;
using Policy = model::Settings::SharedEditPolicy;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";
const std::optional<int> melodic;

CellValues cell (int pitch, bool on, int velocity, double gate)
{
    CellValues values;
    values.pitch = pitch;
    values.on = on;
    values.velocity = velocity;
    values.gate = gate;
    return values;
}

// The command fixture with what these tests need on top: step clips, and
// references to them, each made through its own command. The commands under
// test are here once more as gestures at pool level, each with a context of
// its own.
struct Song : CommandFixture
{
    juce::String trackId (int position) const
    {
        return model::TrackModel (track (position)).getId();
    }

    // A step clip with the schema's default grid: sixteen columns of 240
    // ticks, a loop of 3840, and no lane.
    juce::String stepClip()
    {
        const auto result = commands::addStepClip (project, "Steps", undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    juce::String eventClip()
    {
        const auto result = commands::addEventClip (project, "Clip", 3840, undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    // A placement of one bar on that track; the result is its id.
    juce::String place (int trackPosition, const juce::String& clipId, std::int64_t start)
    {
        const auto result = commands::addPlacement (project, trackId (trackPosition), clipId, start, 3840, undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    // One bar of the clip on that track, split so that its second half has
    // this offset; the result is the id of the second half.
    juce::String placeWithOffset (int trackPosition, const juce::String& clipId, std::int64_t offset)
    {
        const auto result = commands::splitPlacement (project, place (trackPosition, clipId, 0), offset, undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    juce::ValueTree clipNode (const juce::String& clipId) const
    {
        return commands::NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId);
    }

    model::ClipModel clip (const juce::String& clipId) const
    {
        return model::ClipModel (clipNode (clipId));
    }

    juce::ValueTree gridNode (const juce::String& clipId) const
    {
        return clipNode (clipId).getChildWithName (ids::Grid);
    }

    model::GridModel grid (const juce::String& clipId) const
    {
        return clip (clipId).getGrid();
    }

    juce::ValueTree cellNode (const juce::String& clipId, int laneIndex, int column) const
    {
        const auto lane = commands::NodeLookup::childrenOfType (gridNode (clipId), ids::Lane)[static_cast<std::size_t> (laneIndex)];
        return commands::NodeLookup::childrenOfType (lane, ids::Cell)[static_cast<std::size_t> (column)];
    }

    model::CellModel cellAt (const juce::String& clipId, int laneIndex, int column) const
    {
        return model::CellModel (cellNode (clipId, laneIndex, column));
    }

    // The names of the clip's lanes from top to bottom, as one text.
    std::string laneNames (const juce::String& clipId) const
    {
        std::string names;

        for (const auto& lane : grid (clipId).getLanes())
            names += lane.getName().toStdString() + " ";

        return names;
    }

    int clipCount() const
    {
        return static_cast<int> (model::ClipPoolModel (project.getChildWithName (ids::ClipPool)).getClips().size());
    }

    std::int64_t offsetOf (const juce::String& placementId) const
    {
        return model::PlacementModel (commands::NodeLookup::placementWithId (project, placementId)).getOffset();
    }

    // What the tests before the command under test have done is not what
    // its undo is about.
    juce::String startGesture()
    {
        REQUIRE (isValid());
        undoManager.clearUndoHistory();
        undoManager.beginNewTransaction();
        return xml();
    }

    CommandResult addLane (const juce::String& clipId, const juce::String& name, std::optional<int> fixedPitch = {})
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::addLane (project, clipId, name, fixedPitch, poolLevel, undoManager);
    }

    CommandResult removeLane (const juce::String& clipId, int laneIndex)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::removeLane (project, clipId, laneIndex, poolLevel, undoManager);
    }

    CommandResult moveLane (const juce::String& clipId, int laneIndex, int toPosition)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::moveLane (project, clipId, laneIndex, toPosition, poolLevel, undoManager);
    }

    CommandResult setLaneName (const juce::String& clipId, int laneIndex, const juce::String& name)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setLaneName (project, clipId, laneIndex, name, poolLevel, undoManager);
    }

    CommandResult setLaneFixedPitch (const juce::String& clipId, int laneIndex, std::optional<int> fixedPitch)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setLaneFixedPitch (project, clipId, laneIndex, fixedPitch, poolLevel, undoManager);
    }

    CommandResult setCell (const juce::String& clipId, int laneIndex, int column, const CellValues& values)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setCell (project, clipId, laneIndex, column, values, poolLevel, undoManager);
    }

    CommandResult setCellCc (const juce::String& clipId, int laneIndex, int column, int cc, int value)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setCellCc (project, clipId, laneIndex, column, cc, value, poolLevel, undoManager);
    }

    CommandResult removeCellCc (const juce::String& clipId, int laneIndex, int column, int cc)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::removeCellCc (project, clipId, laneIndex, column, cc, poolLevel, undoManager);
    }

    CommandResult setStepLength (const juce::String& clipId, std::int64_t stepLength)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setStepLength (project, clipId, stepLength, poolLevel, undoManager);
    }

    CommandResult setStepCount (const juce::String& clipId, int stepCount)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setStepCount (project, clipId, stepCount, poolLevel, undoManager);
    }

    CommandResult setResetAt (const juce::String& clipId, int resetAt)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setResetAt (project, clipId, resetAt, poolLevel, undoManager);
    }

    CommandResult setColumnSkipped (const juce::String& clipId, int column, bool isSkipped)
    {
        EditContext poolLevel (Policy::updateAll);
        return commands::setColumnSkipped (project, clipId, column, isSkipped, poolLevel, undoManager);
    }

    // A step clip with one melodic lane named "Bass".
    juce::String stepClipWithALane()
    {
        const auto clipId = stepClip();
        REQUIRE (addLane (clipId, "Bass").wasApplied());
        return clipId;
    }
};

} // namespace

//==============================================================================
// Lanes.

TEST_CASE ("addLane puts a lane of rests below the others, and undo takes it away again", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClip();
    REQUIRE (song.addLane (clipId, "Kick", 36).wasApplied());

    const auto before = song.startGesture();
    const auto result = song.addLane (clipId, "Bass");

    REQUIRE (result.wasApplied());
    CHECK (song.isValid());
    CHECK (song.laneNames (clipId) == "Kick Bass ");

    const auto lanes = song.grid (clipId).getLanes();
    CHECK (lanes[0].getFixedPitch() == std::optional<int> (36));
    CHECK (lanes[1].getId() == result.createdId);
    CHECK (! lanes[1].getFixedPitch().has_value());
    REQUIRE (lanes[1].getCells().size() == 16);

    // A cell that stores nothing is the rest the schema defaults.
    CHECK (song.cellNode (clipId, 1, 15).getNumProperties() == 0);
    CHECK (! lanes[1].getCells()[0].isOn());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (! song.undoManager.canUndo());
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("A new lane has as many cells as the grid has columns", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClip();
    REQUIRE (song.setStepCount (clipId, 5).wasApplied());
    REQUIRE (song.addLane (clipId, "").wasApplied());

    CHECK (song.grid (clipId).getLanes()[0].getCells().size() == 5);
    CHECK (song.isValid());
}

TEST_CASE ("A fixed pitch is 0 to 127 or none", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.addLane (clipId, "Low", 0).wasApplied());
    REQUIRE (song.addLane (clipId, "High", 127).wasApplied());

    const auto before = song.startGesture();

    for (const auto pitch : { -1, 128, 1000, std::numeric_limits<int>::min() })
    {
        CHECK (! song.addLane (clipId, "No", pitch).wasApplied());
        CHECK (! song.setLaneFixedPitch (clipId, 0, pitch).wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("setLaneFixedPitch makes a drum row and a melodic row again, and the cells keep their pitches", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCell (clipId, 0, 3, cell (48, true, 100, 0.5)).wasApplied());

    const auto before = song.startGesture();

    REQUIRE (song.setLaneFixedPitch (clipId, 0, 42).wasApplied());
    CHECK (song.grid (clipId).getLanes()[0].getFixedPitch() == std::optional<int> (42));
    CHECK (song.cellAt (clipId, 0, 3).getPitch() == 48);
    CHECK (song.isValid());

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());

    // The pitch it has changes nothing.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setLaneFixedPitch (clipId, 0, 42).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);

    // Without one the property is gone, not empty.
    REQUIRE (song.setLaneFixedPitch (clipId, 0, melodic).wasApplied());
    CHECK (song.xml() == before);
    CHECK (song.isValid());

    song.undoManager.beginNewTransaction();
    REQUIRE (song.setLaneFixedPitch (clipId, 0, melodic).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
}

TEST_CASE ("Undo of a fixed pitch taken away brings the lane back to the letter, wherever the property stood", "[model][commands][StepClipCommands]")
{
    // Found by the property tests at seed 5000: undo of a removed property
    // puts it behind the node's others. Here it stands before the name.
    Song song;
    const auto clipId = song.stepClip();
    REQUIRE (song.setStepCount (clipId, 2).wasApplied());

    const auto lane = juce::ValueTree::fromXml (
        R"(<Lane id="7f9b1d3f5b724e40bd9f7b5d3f4b6c8e" fixedPitch="36" name="Kick">
             <Cell on="1"><CcValue cc="74" value="9"/></Cell><Cell/>
           </Lane>)");
    song.gridNode (clipId).appendChild (lane, nullptr);
    REQUIRE (song.addLane (clipId, "Below").wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.setLaneFixedPitch (clipId, 0, melodic).wasApplied());

    const auto lanes = song.grid (clipId).getLanes();
    REQUIRE (lanes.size() == 2);
    CHECK (! lanes[0].getFixedPitch().has_value());
    CHECK (lanes[0].getId() == "7f9b1d3f5b724e40bd9f7b5d3f4b6c8e");
    CHECK (lanes[0].getName() == "Kick");
    CHECK (song.cellAt (clipId, 0, 0).isOn());
    CHECK (song.cellAt (clipId, 0, 0).getCcValues().size() == 1);
    CHECK (song.laneNames (clipId) == "Kick Below ");
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("setLaneName names the lane at that position and no other", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.addLane (clipId, "Hat", 42).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.setLaneName (clipId, 1, "Open <hat> & \"more\"").wasApplied());

    CHECK (song.laneNames (clipId) == "Bass Open <hat> & \"more\" ");
    CHECK (song.isValid());
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);

    // The name it has changes nothing, and an empty name is a name.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setLaneName (clipId, 1, "Hat").wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
    REQUIRE (song.setLaneName (clipId, 1, "").wasApplied());
    CHECK (song.grid (clipId).getLanes()[1].getName().isEmpty());
    CHECK (song.isValid());
}

TEST_CASE ("removeLane takes out the lane at that position with its cells, down to no lane at all", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClip();

    for (const auto* name : { "A", "B", "C" })
        REQUIRE (song.addLane (clipId, name).wasApplied());

    REQUIRE (song.setCell (clipId, 1, 0, cell (40, true, 90, 1.0)).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.removeLane (clipId, 1).wasApplied());

    CHECK (song.laneNames (clipId) == "A C ");
    CHECK (song.isValid());
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (song.cellAt (clipId, 1, 0).getPitch() == 40);
    REQUIRE (song.undoManager.redo());

    REQUIRE (song.removeLane (clipId, 0).wasApplied());
    REQUIRE (song.removeLane (clipId, 0).wasApplied());
    CHECK (song.grid (clipId).getLanes().empty());
    CHECK (song.isValid());
    CHECK (! song.removeLane (clipId, 0).wasApplied());
}

TEST_CASE ("moveLane moves a lane up and down among the others", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClip();

    for (const auto* name : { "A", "B", "C", "D" })
        REQUIRE (song.addLane (clipId, name).wasApplied());

    const auto before = song.startGesture();

    REQUIRE (song.moveLane (clipId, 0, 2).wasApplied());
    CHECK (song.laneNames (clipId) == "B C A D ");
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());

    REQUIRE (song.moveLane (clipId, 3, 0).wasApplied());
    CHECK (song.laneNames (clipId) == "D B C A ");
    REQUIRE (song.moveLane (clipId, 1, 3).wasApplied());
    CHECK (song.laneNames (clipId) == "D C A B ");
    CHECK (song.isValid());

    // To where it stands: nothing to undo.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.moveLane (clipId, 2, 2).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
}

TEST_CASE ("A position at which the clip has no lane, or to which none can move, is refused", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.addLane (clipId, "Second").wasApplied());

    const auto before = song.startGesture();

    for (const auto position : { -1, 2, 9, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() })
    {
        CHECK (! song.removeLane (clipId, position).wasApplied());
        CHECK (! song.moveLane (clipId, position, 0).wasApplied());
        CHECK (! song.moveLane (clipId, 0, position).wasApplied());
        CHECK (! song.setLaneName (clipId, position, "No").wasApplied());
        CHECK (! song.setLaneFixedPitch (clipId, position, 60).wasApplied());
        CHECK (! song.setCell (clipId, position, 0, CellValues()).wasApplied());
        CHECK (! song.setCellCc (clipId, position, 0, 74, 1).wasApplied());
        CHECK (! song.removeCellCc (clipId, position, 0, 74).wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

//==============================================================================
// Cells.

TEST_CASE ("setCell gives a cell its values, and undo takes them back", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto before = song.startGesture();

    REQUIRE (song.setCell (clipId, 0, 2, cell (43, true, 110, 1.2)).wasApplied());

    const auto stored = song.cellAt (clipId, 0, 2);
    CHECK (stored.getPitch() == 43);
    CHECK (stored.isOn());
    CHECK (stored.getVelocity() == 110);
    CHECK (juce::exactlyEqual (stored.getGate(), 1.2));
    CHECK (song.cellNode (clipId, 0, 1).getNumProperties() == 0);
    CHECK (song.cellNode (clipId, 0, 3).getNumProperties() == 0);
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("setCell takes the whole range of a cell's values", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();

    REQUIRE (song.setCell (clipId, 0, 0, cell (0, true, 1, 4.0)).wasApplied());
    REQUIRE (song.setCell (clipId, 0, 15, cell (127, false, 127, 0.00001)).wasApplied());

    CHECK (juce::exactlyEqual (song.cellAt (clipId, 0, 0).getGate(), 4.0));
    CHECK (juce::exactlyEqual (song.cellAt (clipId, 0, 15).getGate(), 0.00001));
    CHECK (song.isValid());
}

TEST_CASE ("setCell stores a gate as its saved text, six significant digits [D14]", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();

    REQUIRE (song.setCell (clipId, 0, 0, cell (60, true, 100, 1.0 / 3.0)).wasApplied());

    const auto stored = song.cellNode (clipId, 0, 0).getProperty (ids::gate);
    CHECK (stored.isString());
    CHECK (stored.toString() == "0.333333");
    CHECK (juce::exactlyEqual (song.cellAt (clipId, 0, 0).getGate(), 0.333333));
    CHECK (song.isValid());

    // The same value with more digits is the value the cell has.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setCell (clipId, 0, 0, cell (60, true, 100, 0.33333312345)).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);

    // The range is asked of the rounded value: this one is 4.
    REQUIRE (song.setCell (clipId, 0, 1, cell (60, true, 100, 4.0000001)).wasApplied());
    CHECK (song.cellNode (clipId, 0, 1).getProperty (ids::gate).toString() == "4");
    CHECK (song.isValid());
}

TEST_CASE ("setCell stores only what differs, and the values the cell has change nothing", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();

    // The defaults of a cell that stores nothing.
    song.startGesture();
    REQUIRE (song.setCell (clipId, 0, 4, CellValues()).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
    CHECK (song.cellNode (clipId, 0, 4).getNumProperties() == 0);

    REQUIRE (song.setCell (clipId, 0, 4, cell (60, true, 100, 0.5)).wasApplied());
    const auto node = song.cellNode (clipId, 0, 4);
    CHECK (node.getNumProperties() == 1);
    CHECK (node.hasProperty (ids::on));

    REQUIRE (song.setCell (clipId, 0, 4, cell (60, true, 100, 0.75)).wasApplied());
    CHECK (node.getNumProperties() == 2);
    CHECK (node.hasProperty (ids::gate));
    CHECK (song.isValid());
}

TEST_CASE ("setCell refuses values that are not a cell's and changes nothing", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto before = song.startGesture();

    const auto infinity = std::numeric_limits<double>::infinity();

    const std::vector<CellValues> wrong {
        cell (-1, true, 100, 0.5),
        cell (128, true, 100, 0.5),
        cell (60, true, 0, 0.5),
        cell (60, true, 128, 0.5),
        cell (60, true, 100, 0.0),
        cell (60, true, 100, -0.0),
        cell (60, true, 100, -0.5),
        cell (60, true, 100, 4.00001),
        cell (60, true, 100, 100.0),
        cell (60, true, 100, infinity),
        cell (60, true, 100, -infinity),
        cell (60, true, 100, std::numeric_limits<double>::quiet_NaN()),
    };

    for (const auto& values : wrong)
    {
        const auto result = song.setCell (clipId, 0, 0, values);
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("A column the grid does not have is refused", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto before = song.startGesture();

    for (const auto column : { -1, 16, 64, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() })
    {
        CHECK (! song.setCell (clipId, 0, column, CellValues()).wasApplied());
        CHECK (! song.setCellCc (clipId, 0, column, 74, 1).wasApplied());
        CHECK (! song.removeCellCc (clipId, 0, column, 74).wasApplied());
        CHECK (! song.setColumnSkipped (clipId, column, true).wasApplied());
        CHECK (! song.setColumnSkipped (clipId, column, false).wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

//==============================================================================
// CC values.

TEST_CASE ("setCellCc adds a value behind the cell's others and changes the one that is there", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCellCc (clipId, 0, 4, 74, 90).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.setCellCc (clipId, 0, 4, 71, 0).wasApplied());

    auto ccValues = song.cellAt (clipId, 0, 4).getCcValues();
    REQUIRE (ccValues.size() == 2);
    CHECK (ccValues[0].getCc() == 74);
    CHECK (ccValues[1].getCc() == 71);
    CHECK (ccValues[1].getValue() == 0);
    CHECK (song.cellAt (clipId, 0, 5).getCcValues().empty());
    CHECK (song.isValid());

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());

    song.undoManager.beginNewTransaction();
    REQUIRE (song.setCellCc (clipId, 0, 4, 74, 127).wasApplied());
    ccValues = song.cellAt (clipId, 0, 4).getCcValues();
    REQUIRE (ccValues.size() == 2);
    CHECK (ccValues[0].getValue() == 127);
    CHECK (song.isValid());

    // The value it has changes nothing.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setCellCc (clipId, 0, 4, 74, 127).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
}

TEST_CASE ("setCellCc refuses a controller number or value outside 0 to 127", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCellCc (clipId, 0, 0, 0, 0).wasApplied());
    REQUIRE (song.setCellCc (clipId, 0, 0, 127, 127).wasApplied());

    const auto before = song.startGesture();

    for (const auto number : { -1, 128, 1000, std::numeric_limits<int>::min() })
    {
        CHECK (! song.setCellCc (clipId, 0, 0, number, 64).wasApplied());
        CHECK (! song.setCellCc (clipId, 0, 0, 74, number).wasApplied());
        CHECK (! song.setCellCc (clipId, 0, 0, 0, number).wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("removeCellCc takes out the value for that controller, and refuses where there is none", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCellCc (clipId, 0, 4, 74, 90).wasApplied());
    REQUIRE (song.setCellCc (clipId, 0, 4, 71, 10).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.removeCellCc (clipId, 0, 4, 74).wasApplied());

    const auto ccValues = song.cellAt (clipId, 0, 4).getCcValues();
    REQUIRE (ccValues.size() == 1);
    CHECK (ccValues[0].getCc() == 71);
    CHECK (song.isValid());

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);

    for (const auto controller : { 74 + 1, -1, 128 })
        CHECK (! song.removeCellCc (clipId, 0, 4, controller).wasApplied());

    CHECK (! song.removeCellCc (clipId, 0, 5, 74).wasApplied());
    CHECK (song.xml() == before);
}

//==============================================================================
// The grid.

TEST_CASE ("setStepLength sets the ticks per column and brings the offsets of the clip's placements into the new loop, in one undo step", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto otherClipId = song.stepClip();
    const auto second = song.placeWithOffset (0, clipId, 1000);
    const auto ofOther = song.placeWithOffset (1, otherClipId, 1000);

    const auto before = song.startGesture();

    // Sixteen columns of 60 ticks: a loop of 960.
    REQUIRE (song.setStepLength (clipId, 60).wasApplied());

    CHECK (song.grid (clipId).getStepLength() == 60);
    CHECK (song.clip (clipId).getLoopLength() == 960);
    CHECK (song.offsetOf (second) == 40);
    CHECK (song.offsetOf (ofOther) == 1000);
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (song.offsetOf (second) == 1000);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);

    // A longer loop leaves an offset where it is.
    REQUIRE (song.setStepLength (clipId, 3840).wasApplied());
    CHECK (song.offsetOf (second) == 40);

    // 16 ticks: the offset 40 mod 16.
    REQUIRE (song.setStepLength (clipId, 1).wasApplied());
    CHECK (song.offsetOf (second) == 8);
    CHECK (song.isValid());
}

TEST_CASE ("setStepLength refuses a step of no tick or of more than 3840, and the length the grid has changes nothing", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto before = song.startGesture();

    for (const std::int64_t stepLength : { std::int64_t { 0 }, std::int64_t { -1 }, std::int64_t { -240 }, std::int64_t { 3841 }, std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min() })
        CHECK (! song.setStepLength (clipId, stepLength).wasApplied());

    REQUIRE (song.setStepLength (clipId, 240).wasApplied());
    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("setStepCount grows every lane by rests and the skips by columns that play", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.addLane (clipId, "Hat", 42).wasApplied());
    REQUIRE (song.setStepCount (clipId, 4).wasApplied());
    REQUIRE (song.setColumnSkipped (clipId, 1, true).wasApplied());
    REQUIRE (song.setCell (clipId, 1, 3, cell (60, true, 70, 0.5)).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.setStepCount (clipId, 6).wasApplied());

    const auto grid = song.grid (clipId);
    CHECK (grid.getStepCount() == 6);
    CHECK (grid.getSkipMask() == "010000");
    CHECK (grid.getResetAt() == 6);
    CHECK (grid.getLoopLength() == 5 * 240);

    for (const auto& lane : grid.getLanes())
        CHECK (lane.getCells().size() == 6);

    CHECK (song.cellAt (clipId, 1, 3).getVelocity() == 70);
    CHECK (song.cellNode (clipId, 1, 5).getNumProperties() == 0);
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("setStepCount shrinks every lane by its last cells, with what they held", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setColumnSkipped (clipId, 14, true).wasApplied());
    REQUIRE (song.setCell (clipId, 0, 2, cell (43, true, 100, 0.5)).wasApplied());
    REQUIRE (song.setCell (clipId, 0, 12, cell (50, true, 100, 0.5)).wasApplied());
    REQUIRE (song.setCellCc (clipId, 0, 12, 74, 9).wasApplied());

    const auto before = song.startGesture();
    REQUIRE (song.setStepCount (clipId, 3).wasApplied());

    const auto grid = song.grid (clipId);
    CHECK (grid.getStepCount() == 3);
    CHECK (grid.getSkipMask() == "000");
    CHECK (grid.getLanes()[0].getCells().size() == 3);
    CHECK (song.cellAt (clipId, 0, 2).getPitch() == 43);
    CHECK (song.isValid());

    // Growing again brings rests, not what was there.
    REQUIRE (song.setStepCount (clipId, 16).wasApplied());
    CHECK (song.cellNode (clipId, 0, 12).getNumProperties() == 0);
    CHECK (song.cellAt (clipId, 0, 12).getCcValues().empty());
    CHECK (song.grid (clipId).getSkipMask() == "0000000000000000");
    CHECK (song.isValid());

    // Undo does.
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (song.cellAt (clipId, 0, 12).getPitch() == 50);
}

TEST_CASE ("With the step count a reset at the end stays at the end, and any other is kept or comes to the new end", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();

    // Nothing stored: the reset is the step count.
    REQUIRE (song.setStepCount (clipId, 8).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 8);
    CHECK (! song.gridNode (clipId).hasProperty (ids::resetAt));

    // Stored, and at the end: a clip in this session behaves like the same
    // clip saved and loaded, whose reset the writer has left out.
    REQUIRE (song.setResetAt (clipId, 6).wasApplied());
    REQUIRE (song.setResetAt (clipId, 8).wasApplied());
    REQUIRE (song.setStepCount (clipId, 12).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 12);
    REQUIRE (song.setStepCount (clipId, 10).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 10);

    // Before the end: kept while the grid grows and while it still has the
    // column, then at the new end.
    REQUIRE (song.setResetAt (clipId, 6).wasApplied());
    REQUIRE (song.setStepCount (clipId, 16).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 6);
    REQUIRE (song.setStepCount (clipId, 7).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 6);
    REQUIRE (song.setStepCount (clipId, 4).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 4);

    // And from then on it is a reset at the end.
    REQUIRE (song.setStepCount (clipId, 9).wasApplied());
    CHECK (song.grid (clipId).getResetAt() == 9);
    CHECK (song.isValid());
}

TEST_CASE ("setStepCount refuses a count outside 1 to 64 and one that leaves no column to play, and the count the grid has changes nothing", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setStepCount (clipId, 64).wasApplied());
    REQUIRE (song.setStepCount (clipId, 1).wasApplied());
    REQUIRE (song.setStepCount (clipId, 4).wasApplied());

    // Only the last column plays.
    for (const auto column : { 0, 1, 2 })
        REQUIRE (song.setColumnSkipped (clipId, column, true).wasApplied());

    const auto before = song.startGesture();

    for (const auto stepCount : { 0, -1, 65, 1000, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() })
        CHECK (! song.setStepCount (clipId, stepCount).wasApplied());

    for (const auto stepCount : { 3, 2, 1 })
    {
        const auto result = song.setStepCount (clipId, stepCount);
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    REQUIRE (song.setStepCount (clipId, 4).wasApplied());
    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("setStepCount brings the offsets of the clip's placements into the new loop", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto second = song.placeWithOffset (0, clipId, 3000);

    const auto before = song.startGesture();

    // Five columns: a loop of 1200.
    REQUIRE (song.setStepCount (clipId, 5).wasApplied());
    CHECK (song.offsetOf (second) == 600);
    CHECK (song.isValid());

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());

    // A longer loop leaves the offset where it is.
    REQUIRE (song.setStepCount (clipId, 10).wasApplied());
    CHECK (song.offsetOf (second) == 600);

    // Two columns: a loop of 480, and then one of 120. An offset of 0 is
    // not stored.
    REQUIRE (song.setStepCount (clipId, 2).wasApplied());
    CHECK (song.offsetOf (second) == 120);
    REQUIRE (song.setStepLength (clipId, 60).wasApplied());
    CHECK (song.offsetOf (second) == 0);
    CHECK (! commands::NodeLookup::placementWithId (song.project, second).hasProperty (ids::offset));
    CHECK (song.isValid());
}

TEST_CASE ("setResetAt ends the loop before that column, keeps the cells behind it and brings the offsets into the loop", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCell (clipId, 0, 10, cell (50, true, 100, 0.5)).wasApplied());
    const auto second = song.placeWithOffset (0, clipId, 3000);

    const auto before = song.startGesture();
    REQUIRE (song.setResetAt (clipId, 6).wasApplied());

    CHECK (song.grid (clipId).getResetAt() == 6);
    CHECK (song.clip (clipId).getLoopLength() == 1440);
    CHECK (song.offsetOf (second) == 120);
    CHECK (song.grid (clipId).getLanes()[0].getCells().size() == 16);
    CHECK (song.cellAt (clipId, 0, 10).getPitch() == 50);
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);

    // The reset it has changes nothing.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setResetAt (clipId, 6).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);

    REQUIRE (song.setResetAt (clipId, 1).wasApplied());
    CHECK (song.offsetOf (second) == 120);
    REQUIRE (song.setResetAt (clipId, 16).wasApplied());
    CHECK (song.isValid());
}

TEST_CASE ("setResetAt refuses a reset outside 1 to the step count and one before which nothing plays", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setColumnSkipped (clipId, 0, true).wasApplied());
    REQUIRE (song.setColumnSkipped (clipId, 1, true).wasApplied());

    const auto before = song.startGesture();

    for (const auto resetAt : { 0, -1, 17, 64, std::numeric_limits<int>::min(), std::numeric_limits<int>::max() })
        CHECK (! song.setResetAt (clipId, resetAt).wasApplied());

    // Columns 0 and 1 take no time.
    CHECK (! song.setResetAt (clipId, 1).wasApplied());
    CHECK (! song.setResetAt (clipId, 2).wasApplied());
    CHECK (song.isUntouchedSince (before));

    REQUIRE (song.setResetAt (clipId, 3).wasApplied());
    CHECK (song.clip (clipId).getLoopLength() == 240);
    CHECK (song.isValid());
}

TEST_CASE ("setColumnSkipped takes a column out of time and puts it back, and brings the offsets into the loop", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setStepCount (clipId, 8).wasApplied());
    REQUIRE (song.setCell (clipId, 0, 3, cell (48, true, 100, 0.5)).wasApplied());
    const auto second = song.placeWithOffset (0, clipId, 1800);

    const auto before = song.startGesture();
    REQUIRE (song.setColumnSkipped (clipId, 3, true).wasApplied());

    CHECK (song.grid (clipId).getSkipMask() == "00010000");
    CHECK (song.grid (clipId).isColumnSkipped (3));
    CHECK (song.clip (clipId).getLoopLength() == 1680);
    CHECK (song.offsetOf (second) == 120);
    CHECK (song.cellAt (clipId, 0, 3).getPitch() == 48);
    CHECK (song.isValid());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);

    // What it is already changes nothing.
    song.undoManager.beginNewTransaction();
    REQUIRE (song.setColumnSkipped (clipId, 3, true).wasApplied());
    REQUIRE (song.setColumnSkipped (clipId, 4, false).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);

    REQUIRE (song.setColumnSkipped (clipId, 3, false).wasApplied());
    CHECK (song.grid (clipId).getSkipMask() == "00000000");
    CHECK (song.clip (clipId).getLoopLength() == 1920);
    CHECK (song.offsetOf (second) == 120);
    CHECK (song.isValid());
}

TEST_CASE ("The last column that plays cannot be skipped, and a column behind the reset can", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setStepCount (clipId, 4).wasApplied());
    REQUIRE (song.setResetAt (clipId, 2).wasApplied());
    REQUIRE (song.setColumnSkipped (clipId, 0, true).wasApplied());

    const auto before = song.startGesture();

    const auto result = song.setColumnSkipped (clipId, 1, true);
    CHECK (! result.wasApplied());
    CHECK (result.refusal.isNotEmpty());
    CHECK (song.isUntouchedSince (before));

    // Columns 2 and 3 are not in the loop; skipping them changes no length.
    REQUIRE (song.setColumnSkipped (clipId, 2, true).wasApplied());
    REQUIRE (song.setColumnSkipped (clipId, 3, true).wasApplied());
    CHECK (song.grid (clipId).getSkipMask() == "1011");
    CHECK (song.clip (clipId).getLoopLength() == 240);
    CHECK (song.isValid());

    // And now the reset cannot go where it would matter less, nor the
    // count: column 1 is all there is.
    CHECK (! song.setStepCount (clipId, 1).wasApplied());
    REQUIRE (song.setResetAt (clipId, 4).wasApplied());
    CHECK (song.clip (clipId).getLoopLength() == 240);
    CHECK (song.isValid());
}

//==============================================================================
// The gate.

TEST_CASE ("The step clip commands refuse an event clip, a clip that is not there and a context of another clip", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.setCellCc (clipId, 0, 0, 74, 1).wasApplied());
    const auto eventClipId = song.eventClip();
    const auto ofTheEventClip = song.place (0, eventClipId, 0);

    const auto before = song.startGesture();

    const auto allRefuse = [&song] (const juce::String& id, EditContext& editContext) {
        auto& project = song.project;
        auto& undoManager = song.undoManager;

        const std::vector<CommandResult> results {
            commands::addLane (project, id, "Lane", melodic, editContext, undoManager),
            commands::removeLane (project, id, 0, editContext, undoManager),
            commands::moveLane (project, id, 0, 0, editContext, undoManager),
            commands::setLaneName (project, id, 0, "Name", editContext, undoManager),
            commands::setLaneFixedPitch (project, id, 0, 36, editContext, undoManager),
            commands::setCell (project, id, 0, 0, cell (36, true, 100, 0.5), editContext, undoManager),
            commands::setCellCc (project, id, 0, 0, 71, 1, editContext, undoManager),
            commands::removeCellCc (project, id, 0, 0, 74, editContext, undoManager),
            commands::setStepLength (project, id, 120, editContext, undoManager),
            commands::setStepCount (project, id, 8, editContext, undoManager),
            commands::setResetAt (project, id, 4, editContext, undoManager),
            commands::setColumnSkipped (project, id, 2, true, editContext, undoManager),
        };

        for (const auto& result : results)
        {
            CHECK (! result.wasApplied());
            CHECK (result.refusal.isNotEmpty());
        }
    };

    EditContext poolLevel (Policy::updateAll);
    allRefuse (eventClipId, poolLevel);
    allRefuse (unknownId, poolLevel);
    allRefuse ("", poolLevel);

    EditContext throughAnother (Policy::makeUnique);
    throughAnother.addPlacement (ofTheEventClip);
    allRefuse (clipId, throughAnother);

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("The commands of one gesture all change its one clone, each naming lanes and cells by position", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    REQUIRE (song.addLane (clipId, "Hat", 42).wasApplied());
    REQUIRE (song.setCellCc (clipId, 1, 2, 74, 5).wasApplied());
    const auto first = song.place (0, clipId, 0);
    const auto second = song.place (1, clipId, 0);

    const auto before = song.startGesture();
    const auto originalXml = song.clipNode (clipId).toXmlString();

    auto& project = song.project;
    auto& undoManager = song.undoManager;
    EditContext throughFirst (Policy::makeUnique);
    throughFirst.addPlacement (first);

    REQUIRE (commands::setCell (project, clipId, 1, 2, cell (60, true, 80, 2.5), throughFirst, undoManager).wasApplied());
    REQUIRE (throughFirst.hasClone());
    const auto cloneId = throughFirst.getCloneId();

    REQUIRE (commands::setCellCc (project, clipId, 1, 2, 74, 99, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setCellCc (project, cloneId, 1, 3, 71, 1, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::removeCellCc (project, clipId, 1, 3, 71, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setLaneName (project, clipId, 1, "Open hat", throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setLaneFixedPitch (project, clipId, 1, 46, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::moveLane (project, clipId, 1, 0, throughFirst, undoManager).wasApplied());
    const auto added = commands::addLane (project, clipId, "Kick", 36, throughFirst, undoManager);
    REQUIRE (added.wasApplied());
    REQUIRE (commands::removeLane (project, clipId, 1, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setStepLength (project, clipId, 120, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setStepCount (project, clipId, 8, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setResetAt (project, clipId, 6, throughFirst, undoManager).wasApplied());
    REQUIRE (commands::setColumnSkipped (project, clipId, 0, true, throughFirst, undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (song.clipCount() == 2);

    // The original is as it was, and the second placement still has it.
    CHECK (song.clipNode (clipId).toXmlString() == originalXml);
    CHECK (model::PlacementModel (commands::NodeLookup::placementWithId (project, second)).getClipId() == clipId);
    CHECK (model::PlacementModel (commands::NodeLookup::placementWithId (project, first)).getClipId() == cloneId);

    CHECK (song.laneNames (cloneId) == "Open hat Kick ");
    const auto cloneGrid = song.grid (cloneId);
    CHECK (cloneGrid.getLanes()[0].getFixedPitch() == std::optional<int> (46));
    CHECK (cloneGrid.getLanes()[1].getId() == added.createdId);
    CHECK (cloneGrid.getLanes()[1].getCells().size() == 8);
    CHECK (cloneGrid.getLoopLength() == 5 * 120);
    CHECK (juce::exactlyEqual (song.cellAt (cloneId, 0, 2).getGate(), 2.5));
    REQUIRE (song.cellAt (cloneId, 0, 2).getCcValues().size() == 1);
    CHECK (song.cellAt (cloneId, 0, 2).getCcValues()[0].getValue() == 99);
    CHECK (song.cellAt (cloneId, 0, 3).getCcValues().empty());

    // One undo takes back the clone and everything done to it.
    REQUIRE (undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (! undoManager.canUndo());
}

TEST_CASE ("Under make unique a change to the grid changes the offsets of the clone's placements only", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto ofTheClone = song.placeWithOffset (0, clipId, 3000);
    const auto ofTheOriginal = song.placeWithOffset (1, clipId, 3000);

    song.startGesture();

    EditContext throughOne (Policy::makeUnique);
    throughOne.addPlacement (ofTheClone);
    REQUIRE (commands::setResetAt (song.project, clipId, 4, throughOne, song.undoManager).wasApplied());

    REQUIRE (throughOne.hasClone());
    CHECK (song.clip (throughOne.getCloneId()).getLoopLength() == 960);
    CHECK (song.clip (clipId).getLoopLength() == 3840);
    CHECK (song.offsetOf (ofTheClone) == 120);
    CHECK (song.offsetOf (ofTheOriginal) == 3000);
    CHECK (song.isValid());
}

TEST_CASE ("Under update all a change through one placement is a change for every reference of the step clip", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto first = song.place (0, clipId, 0);
    const auto second = song.placeWithOffset (1, clipId, 3000);
    REQUIRE (commands::setSlot (song.project, song.trackId (2), 0, clipId, song.undoManager).wasApplied());

    song.startGesture();

    EditContext throughFirst (Policy::updateAll);
    throughFirst.addPlacement (first);
    REQUIRE (commands::setStepCount (song.project, clipId, 5, throughFirst, song.undoManager).wasApplied());

    CHECK (! throughFirst.hasClone());
    CHECK (song.clipCount() == 1);
    CHECK (song.offsetOf (second) == 600);
    CHECK (song.isValid());
}

TEST_CASE ("A step clip command that changes nothing or is refused makes no clone", "[model][commands][StepClipCommands]")
{
    Song song;
    const auto clipId = song.stepClipWithALane();
    const auto first = song.place (0, clipId, 0);
    song.place (1, clipId, 0);

    const auto before = song.startGesture();

    auto& project = song.project;
    auto& undoManager = song.undoManager;
    EditContext throughFirst (Policy::makeUnique);
    throughFirst.addPlacement (first);

    CHECK (commands::setCell (project, clipId, 0, 0, CellValues(), throughFirst, undoManager).wasApplied());
    CHECK (commands::setLaneName (project, clipId, 0, "Bass", throughFirst, undoManager).wasApplied());
    CHECK (commands::setLaneFixedPitch (project, clipId, 0, melodic, throughFirst, undoManager).wasApplied());
    CHECK (commands::moveLane (project, clipId, 0, 0, throughFirst, undoManager).wasApplied());
    CHECK (commands::setStepLength (project, clipId, 240, throughFirst, undoManager).wasApplied());
    CHECK (commands::setStepCount (project, clipId, 16, throughFirst, undoManager).wasApplied());
    CHECK (commands::setResetAt (project, clipId, 16, throughFirst, undoManager).wasApplied());
    CHECK (commands::setColumnSkipped (project, clipId, 0, false, throughFirst, undoManager).wasApplied());

    CHECK (! commands::setCell (project, clipId, 0, 0, cell (60, true, 100, 5.0), throughFirst, undoManager).wasApplied());
    CHECK (! commands::setCellCc (project, clipId, 0, 0, 128, 0, throughFirst, undoManager).wasApplied());
    CHECK (! commands::removeCellCc (project, clipId, 0, 0, 74, throughFirst, undoManager).wasApplied());
    CHECK (! commands::setStepCount (project, clipId, 65, throughFirst, undoManager).wasApplied());

    CHECK (! throughFirst.hasClone());
    CHECK (song.isUntouchedSince (before));
}
