// model::commands for the session grid: what is in a slot, and the scenes
// as rows that are inserted, removed and moved (docs/SCHEMA.md -> Slots /
// Slot, Session).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/SessionModel.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/SessionCommands.h"
#include "model/commands/SlotCommands.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";
const int largestRow = std::numeric_limits<int>::max();

juce::String idOfTrack (const CommandFixture& fixture, int position)
{
    return model::TrackModel (fixture.track (position)).getId();
}

int refcountOf (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getRefcount (clipId);
}

// The fixture with three clips, named a, b and c, to put into slots.
struct SessionFixture : CommandFixture
{
    SessionFixture()
    {
        for (const auto* name : { "a", "b", "c" })
        {
            const auto result = commands::addEventClip (project, name, 3840, undoManager);
            REQUIRE (result.wasApplied());
            clipIds.push_back (result.createdId);
            namesById[result.createdId] = name;
        }

        undoManager.clearUndoHistory();
    }

    // Puts the clip with that name into a slot.
    void fill (int trackPosition, int scene, const std::string& clipName)
    {
        const auto clipId = std::find_if (namesById.begin(), namesById.end(), [&clipName] (const auto& entry) { return entry.second == clipName; })->first;

        REQUIRE (commands::setSlot (project, idOfTrack (*this, trackPosition), scene, clipId, undoManager).wasApplied());
    }

    // The whole grid in row order: every occupied slot as
    // "row:track=clip", every Scene node as "row:'name'".
    std::string grid() const
    {
        std::vector<std::pair<int, std::string>> entries;

        for (int position = 0; position < 3; ++position)
            for (const auto& slot : model::TrackModel (track (position)).getSlots())
                entries.push_back ({ slot.getScene(), std::to_string (slot.getScene()) + ":" + std::to_string (position) + "=" + namesById.at (slot.getClipId()) });

        for (const auto& scene : model::SessionModel (session()).getScenes())
            entries.push_back ({ scene.getIndex(), std::to_string (scene.getIndex()) + ":'" + scene.getName().toStdString() + "'" });

        std::stable_sort (entries.begin(), entries.end(), [] (const auto& one, const auto& other) { return one.first < other.first; });

        std::string text;

        for (const auto& entry : entries)
            text += entry.second + " ";

        return text;
    }

    // Slots on rows 0, 1, 2 and 4, beside the fixture's scene "Chorus" in
    // row 2; row 3 is empty.
    void fillGrid()
    {
        fill (0, 0, "a");
        fill (1, 0, "b");
        fill (0, 1, "b");
        fill (0, 2, "c");
        fill (2, 2, "a");
        fill (1, 4, "c");
        REQUIRE (commands::setSceneName (project, 0, "Intro", undoManager).wasApplied());
        REQUIRE (isValid());
        REQUIRE (grid() == "0:0=a 0:1=b 0:'Intro' 1:0=b 2:0=c 2:2=a 2:'Chorus' 4:1=c ");

        undoManager.clearUndoHistory();
    }

    std::vector<juce::String> clipIds;
    std::map<juce::String, std::string> namesById;
};

} // namespace

TEST_CASE ("setSlot puts a clip into a slot, and undo empties it again", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setSlot (fixture.project, idOfTrack (fixture, 1), 3, fixture.clipIds[0], fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "2:'Chorus' 3:1=a ");
    CHECK (refcountOf (fixture, fixture.clipIds[0]) == 1);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("One clip may be in any number of slots, also of one track; a step clip too", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    const auto stepClip = commands::addStepClip (fixture.project, "s", fixture.undoManager);
    REQUIRE (stepClip.wasApplied());
    fixture.namesById[stepClip.createdId] = "s";

    fixture.fill (0, 0, "a");
    fixture.fill (0, 1, "a");
    fixture.fill (1, 0, "a");
    fixture.fill (2, 0, "s");

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=a 0:2=s 1:0=a 2:'Chorus' ");
    CHECK (refcountOf (fixture, fixture.clipIds[0]) == 3);
}

TEST_CASE ("setSlot on an occupied slot replaces its clip, in one undo step", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fill (0, 1, "a");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setSlot (fixture.project, idOfTrack (fixture, 0), 1, fixture.clipIds[1], fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "1:0=b 2:'Chorus' ");
    CHECK (refcountOf (fixture, fixture.clipIds[0]) == 0);
    CHECK (refcountOf (fixture, fixture.clipIds[1]) == 1);

    // The clip that gave way is still in the pool.
    CHECK (fixture.project.getChildWithName (ids::ClipPool).getNumChildren() == 3);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
}

TEST_CASE ("setSlot with the clip the slot holds is applied and leaves nothing to undo", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fill (0, 1, "a");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (commands::setSlot (fixture.project, idOfTrack (fixture, 0), 1, fixture.clipIds[0], fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setSlot refuses a track or a clip that is not there, and a negative scene", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    const auto before = fixture.xml();
    const auto trackId = idOfTrack (fixture, 0);

    const auto results = {
        commands::setSlot (fixture.project, unknownId, 0, fixture.clipIds[0], fixture.undoManager),
        commands::setSlot (fixture.project, "", 0, fixture.clipIds[0], fixture.undoManager),
        commands::setSlot (fixture.project, trackId, 0, unknownId, fixture.undoManager),
        commands::setSlot (fixture.project, trackId, 0, "", fixture.undoManager),
        // The id of a track names no clip.
        commands::setSlot (fixture.project, trackId, 0, trackId, fixture.undoManager),
        commands::setSlot (fixture.project, trackId, -1, fixture.clipIds[0], fixture.undoManager),
        commands::setSlot (fixture.project, trackId, std::numeric_limits<int>::min(), fixture.clipIds[0], fixture.undoManager),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("The largest row there is can hold a slot", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;

    REQUIRE (commands::setSlot (fixture.project, idOfTrack (fixture, 0), largestRow, fixture.clipIds[0], fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("clearSlot empties a slot and leaves its clip in the pool; undo fills it again", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::clearSlot (fixture.project, idOfTrack (fixture, 0), 1, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 2:0=c 2:2=a 2:'Chorus' 4:1=c ");
    CHECK (refcountOf (fixture, fixture.clipIds[1]) == 1);
    CHECK (fixture.project.getChildWithName (ids::ClipPool).getNumChildren() == 3);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("clearSlot refuses a slot that is empty, and a track that is not there", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    // Track 1 has a slot in rows 0 and 4 only; row 2 has a name, and a slot
    // on other tracks.
    CHECK (! commands::clearSlot (fixture.project, idOfTrack (fixture, 1), 2, fixture.undoManager).wasApplied());
    CHECK (! commands::clearSlot (fixture.project, idOfTrack (fixture, 1), 3, fixture.undoManager).wasApplied());
    CHECK (! commands::clearSlot (fixture.project, idOfTrack (fixture, 1), -1, fixture.undoManager).wasApplied());
    CHECK (! commands::clearSlot (fixture.project, unknownId, 0, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("insertScene moves the row that was there and every later one down, on every track and with their names", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::insertScene (fixture.project, 1, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 2:0=b 3:0=c 3:2=a 3:'Chorus' 5:1=c ");

    // One undo step for the whole renumbering.
    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("insertScene at row 0 moves every row; behind the last row in use it changes nothing", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (commands::insertScene (fixture.project, 5, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));

    REQUIRE (commands::insertScene (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "1:0=a 1:1=b 1:'Intro' 2:0=b 3:0=c 3:2=a 3:'Chorus' 5:1=c ");
}

TEST_CASE ("insertScene refuses a negative row, and any row while the largest row is in use", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (! commands::insertScene (fixture.project, -1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));

    SECTION ("by a slot")
    {
        fixture.fill (2, largestRow, "a");
    }

    SECTION ("by a scene's name")
    {
        REQUIRE (commands::setSceneName (fixture.project, largestRow, "End", fixture.undoManager).wasApplied());
    }

    fixture.undoManager.clearUndoHistory();
    const auto withLargestRow = fixture.xml();

    CHECK (! commands::insertScene (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (! commands::insertScene (fixture.project, largestRow, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (withLargestRow));
}

TEST_CASE ("removeScene empties the row on every track, drops its name and moves the later rows up", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeScene (fixture.project, 2, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 1:0=b 3:1=c ");

    // The clips of the row are still in the pool.
    CHECK (fixture.project.getChildWithName (ids::ClipPool).getNumChildren() == 3);
    CHECK (refcountOf (fixture, fixture.clipIds[0]) == 1);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("removeScene of an empty row still moves the later rows up; behind the last row in use it changes nothing", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (commands::removeScene (fixture.project, 5, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));

    REQUIRE (commands::removeScene (fixture.project, 3, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 1:0=b 2:0=c 2:2=a 2:'Chorus' 3:1=c ");
}

TEST_CASE ("removeScene refuses a negative row", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (! commands::removeScene (fixture.project, -1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("insertScene and then removeScene of the same row give the grid as it was", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.grid();

    for (const auto row : { 0, 1, 2, 3, 4, 5 })
    {
        REQUIRE (commands::insertScene (fixture.project, row, fixture.undoManager).wasApplied());
        REQUIRE (commands::removeScene (fixture.project, row, fixture.undoManager).wasApplied());
        CHECK (fixture.grid() == before);
    }
}

TEST_CASE ("moveScene down takes the row with its slots and its name, and the rows it passes move up", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::moveScene (fixture.project, 0, 2, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=b 1:0=c 1:2=a 1:'Chorus' 2:0=a 2:1=b 2:'Intro' 4:1=c ");

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("moveScene up takes the row with its slots and its name, and the rows it passes move down", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();

    REQUIRE (commands::moveScene (fixture.project, 4, 1, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 1:1=c 2:0=b 3:0=c 3:2=a 3:'Chorus' ");

    // And back again.
    REQUIRE (commands::moveScene (fixture.project, 1, 4, fixture.undoManager).wasApplied());
    CHECK (fixture.grid() == "0:0=a 0:1=b 0:'Intro' 1:0=b 2:0=c 2:2=a 2:'Chorus' 4:1=c ");
}

TEST_CASE ("moveScene of an empty row moves the rows between as the gap moves", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();

    // Row 3 is empty; at row 0 it pushes rows 0 to 2 down.
    REQUIRE (commands::moveScene (fixture.project, 3, 0, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "1:0=a 1:1=b 1:'Intro' 2:0=b 3:0=c 3:2=a 3:'Chorus' 4:1=c ");
}

TEST_CASE ("moveScene to the row it is in is applied and leaves nothing to undo", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (commands::moveScene (fixture.project, 2, 2, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("moveScene works up to the largest row", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fill (0, largestRow, "a");
    fixture.fill (0, largestRow - 1, "b");

    REQUIRE (commands::moveScene (fixture.project, largestRow, 0, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "0:0=a 3:'Chorus' " + std::to_string (largestRow) + ":0=b ");

    REQUIRE (commands::moveScene (fixture.project, 0, largestRow, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (fixture.grid() == "2:'Chorus' " + std::to_string (largestRow - 1) + ":0=b " + std::to_string (largestRow) + ":0=a ");
}

TEST_CASE ("moveScene refuses a negative row on either side", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    fixture.fillGrid();
    const auto before = fixture.xml();

    CHECK (! commands::moveScene (fixture.project, -1, 2, fixture.undoManager).wasApplied());
    CHECK (! commands::moveScene (fixture.project, 2, -1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("The scene commands work on a session with nothing in it", "[model][commands][SlotCommands]")
{
    SessionFixture fixture;
    REQUIRE (commands::removeScene (fixture.project, 2, fixture.undoManager).wasApplied());
    REQUIRE (fixture.grid().empty());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (commands::insertScene (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (commands::removeScene (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (commands::moveScene (fixture.project, 0, 7, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}
