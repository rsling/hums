// model::commands for the clip pool (docs/SCHEMA.md -> ClipPool / Clip).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/ClipPoolModel.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/PlacementCommands.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";

std::vector<model::ClipModel> clipsOf (const CommandFixture& fixture)
{
    return model::ClipPoolModel (fixture.project.getChildWithName (ids::ClipPool)).getClips();
}

// The names of the clips in pool order, as one text.
std::string clipNamesOf (const CommandFixture& fixture)
{
    std::string names;

    for (const auto& clip : clipsOf (fixture))
        names += clip.getName().toStdString() + "|";

    return names;
}

int refcountOf (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getRefcount (clipId);
}

// A placement of 960 ticks on that track, through its own command; the
// result is its id.
juce::String placeClip (CommandFixture& fixture, int trackPosition, const juce::String& clipId, std::int64_t start)
{
    const auto trackId = model::TrackModel (fixture.track (trackPosition)).getId();
    const auto result = commands::addPlacement (fixture.project, trackId, clipId, start, 960, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

// The commands for slots are not what is tested here, so the slot is put
// into the tree as a loaded file would hold it.
void putClipInSlot (CommandFixture& fixture, int trackPosition, const juce::String& clipId, int scene)
{
    juce::ValueTree slot (ids::Slot);
    slot.setProperty (ids::scene, scene, nullptr);
    slot.setProperty (ids::clipId, clipId, nullptr);

    fixture.track (trackPosition).getChildWithName (ids::Slots).appendChild (slot, nullptr);
}

// Name and colour set at pool level: by a gesture of its own that names no
// reference. What a context with references does is in RefcountGateTest.
commands::CommandResult renameInPool (CommandFixture& fixture, const juce::String& clipId, const juce::String& name)
{
    commands::EditContext poolLevel (model::Settings::SharedEditPolicy::updateAll);
    return commands::setClipName (fixture.project, clipId, name, poolLevel, fixture.undoManager);
}

commands::CommandResult colourInPool (CommandFixture& fixture, const juce::String& clipId, const juce::String& colour)
{
    commands::EditContext poolLevel (model::Settings::SharedEditPolicy::updateAll);
    return commands::setClipColour (fixture.project, clipId, colour, poolLevel, fixture.undoManager);
}

juce::String addedEventClip (CommandFixture& fixture, const juce::String& name)
{
    const auto result = commands::addEventClip (fixture.project, name, 3840, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

juce::String addedStepClip (CommandFixture& fixture, const juce::String& name)
{
    const auto result = commands::addStepClip (fixture.project, name, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

} // namespace

TEST_CASE ("addEventClip makes an empty event clip that nothing references, and undo takes it away again", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addEventClip (fixture.project, "Lead line", 7680, fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto clips = clipsOf (fixture);
    REQUIRE (clips.size() == 1);

    const auto& added = clips[0];
    CHECK (added.getId() == result.createdId);
    CHECK (added.getName() == "Lead line");
    CHECK (added.getKind() == model::ClipModel::Kind::event);
    CHECK (added.getLoopLength() == 7680);
    CHECK (added.getQuantise().isEmpty());
    CHECK (added.getColour().isEmpty());
    CHECK (added.getNotes().empty());
    CHECK (added.getEvents().empty());
    CHECK (refcountOf (fixture, result.createdId) == 0);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("addEventClip takes any loop length from one tick on", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;

    CHECK (commands::addEventClip (fixture.project, "", 1, fixture.undoManager).wasApplied());
    CHECK (commands::addEventClip (fixture.project, "", std::numeric_limits<std::int64_t>::max(), fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto clips = clipsOf (fixture);
    REQUIRE (clips.size() == 2);
    CHECK (clips[0].getLoopLength() == 1);
    CHECK (clips[1].getLoopLength() == std::numeric_limits<std::int64_t>::max());
}

TEST_CASE ("addEventClip refuses a loop that has no length", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    for (const auto loopLength : { std::int64_t { 0 }, std::int64_t { -1 }, std::numeric_limits<std::int64_t>::min() })
    {
        const auto result = commands::addEventClip (fixture.project, "x", loopLength, fixture.undoManager);

        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
        CHECK (result.createdId.isEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("addStepClip makes a step clip with the default grid and no lane, and undo takes it away again", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addStepClip (fixture.project, "Bass 16-step", fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto clips = clipsOf (fixture);
    REQUIRE (clips.size() == 1);

    const auto& added = clips[0];
    CHECK (added.getId() == result.createdId);
    CHECK (added.getName() == "Bass 16-step");
    CHECK (added.getKind() == model::ClipModel::Kind::step);
    CHECK (added.getColour().isEmpty());

    const auto grid = added.getGrid();
    CHECK (grid.getStepLength() == 240);
    CHECK (grid.getStepCount() == 16);
    CHECK (grid.getResetAt() == 16);
    CHECK (grid.getSkipMask() == "0000000000000000");
    CHECK (grid.getLanes().empty());

    // Sixteen sixteenths: one bar of four quarters.
    CHECK (added.getLoopLength() == 3840);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("Clips that are added get ids of their own, stand in the pool in the order they were added, and may share a name", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;

    const auto first = addedEventClip (fixture, "Riff");
    const auto second = addedStepClip (fixture, "Riff");
    const auto third = addedEventClip (fixture, "");

    CHECK (first != second);
    CHECK (second != third);
    CHECK (first != third);
    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Riff|Riff||");
}

TEST_CASE ("The name and the colour of a clip are set through commands, for clips of both kinds", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto eventClipId = addedEventClip (fixture, "Lead");
    const auto stepClipId = addedStepClip (fixture, "Bass");

    REQUIRE (renameInPool (fixture, eventClipId, "Lead 2").wasApplied());
    REQUIRE (colourInPool (fixture, eventClipId, "#ff00AA10").wasApplied());
    REQUIRE (renameInPool (fixture, stepClipId, "").wasApplied());
    REQUIRE (colourInPool (fixture, stepClipId, "#80102030").wasApplied());

    CHECK (fixture.isValid());

    auto clips = clipsOf (fixture);
    CHECK (clips[0].getName() == "Lead 2");
    CHECK (clips[0].getColour() == "#ff00AA10");
    CHECK (clips[1].getName().isEmpty());
    CHECK (clips[1].getColour() == "#80102030");

    // Nothing else of the clip has changed.
    CHECK (clips[0].getKind() == model::ClipModel::Kind::event);
    CHECK (clips[0].getLoopLength() == 3840);
    CHECK (clips[1].getKind() == model::ClipModel::Kind::step);

    REQUIRE (colourInPool (fixture, eventClipId, "").wasApplied());
    CHECK (fixture.isValid());
    CHECK (clipsOf (fixture)[0].getColour().isEmpty());
}

TEST_CASE ("A clip's name and colour set at pool level are set for every reference of the clip, whatever its refcount", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Riff");
    placeClip (fixture, 0, clipId, 0);
    placeClip (fixture, 1, clipId, 960);
    putClipInSlot (fixture, 2, clipId, 0);
    REQUIRE (fixture.isValid());
    REQUIRE (refcountOf (fixture, clipId) == 3);

    REQUIRE (renameInPool (fixture, clipId, "Riff B").wasApplied());
    REQUIRE (colourInPool (fixture, clipId, "#ffffffff").wasApplied());

    // No clone: still the one clip, and its three references.
    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Riff B|");
    CHECK (refcountOf (fixture, clipId) == 3);
}

TEST_CASE ("A change of a clip's name or colour is taken back by one undo", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Lead");
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (renameInPool (fixture, clipId, "Solo").wasApplied());
    const auto renamed = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == renamed);
    REQUIRE (fixture.undoManager.undo());

    fixture.undoManager.beginNewTransaction();
    REQUIRE (colourInPool (fixture, clipId, "#ff000000").wasApplied());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("Setting a clip's name or colour to the one it has is applied and leaves nothing to undo", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Lead");
    REQUIRE (colourInPool (fixture, clipId, "#ff000000").wasApplied());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (renameInPool (fixture, clipId, "Lead").wasApplied());
    CHECK (colourInPool (fixture, clipId, "#ff000000").wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setClipColour refuses a text that is no colour", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Lead");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    for (const auto* colour : { "red", "#ff0000", "ffff00000", "#ff00000g", "#ffff000000", "#" })
    {
        const auto result = colourInPool (fixture, clipId, colour);

        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeClip takes an unused clip out of the pool, and undo puts it back where it was", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    addedEventClip (fixture, "A");
    const auto stepClipId = addedStepClip (fixture, "B");
    addedEventClip (fixture, "C");
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeClip (fixture.project, stepClipId, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "A|C|");

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (clipNamesOf (fixture) == "A|B|C|");
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("removeClip refuses a clip that a Placement references", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Riff");
    placeClip (fixture, 0, clipId, 0);
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    const auto result = commands::removeClip (fixture.project, clipId, fixture.undoManager);

    CHECK (! result.wasApplied());
    CHECK (result.refusal.isNotEmpty());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeClip refuses a clip that only a Slot references", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedStepClip (fixture, "Drums");
    putClipInSlot (fixture, 1, clipId, 0);
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (! commands::removeClip (fixture.project, clipId, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeClip refuses a clip that only a placement on an AutomationLane references", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Sweep");

    juce::ValueTree lane (ids::AutomationLane);
    lane.setProperty (ids::id, juce::Uuid().toString(), nullptr);
    juce::ValueTree placement (ids::Placement);
    placement.setProperty (ids::id, juce::Uuid().toString(), nullptr);
    placement.setProperty (ids::clipId, clipId, nullptr);
    placement.setProperty (ids::start, 0, nullptr);
    placement.setProperty (ids::length, 960, nullptr);
    lane.appendChild (placement, nullptr);
    fixture.track (0).getChildWithName (ids::AutomationLanes).appendChild (lane, nullptr);

    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (! commands::removeClip (fixture.project, clipId, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A clip can be removed once its last reference is gone", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Riff");
    const auto placementId = placeClip (fixture, 0, clipId, 0);
    REQUIRE (! commands::removeClip (fixture.project, clipId, fixture.undoManager).wasApplied());

    REQUIRE (commands::removePlacement (fixture.project, placementId, fixture.undoManager).wasApplied());

    CHECK (commands::removeClip (fixture.project, clipId, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (clipsOf (fixture).empty());
}

TEST_CASE ("removeUnusedClips takes every clip out that nothing references, in one undo step, and keeps the others in their order", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    addedEventClip (fixture, "unused 1");
    const auto placedId = addedEventClip (fixture, "placed");
    addedStepClip (fixture, "unused 2");
    const auto slotId = addedStepClip (fixture, "in a slot");
    addedEventClip (fixture, "unused 3");
    placeClip (fixture, 0, placedId, 0);
    putClipInSlot (fixture, 1, slotId, 3);
    REQUIRE (fixture.isValid());
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removeUnusedClips (fixture.project, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "placed|in a slot|");

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (clipNamesOf (fixture) == "unused 1|placed|unused 2|in a slot|unused 3|");
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("removeUnusedClips with no unused clip, or no clip at all, is applied and leaves nothing to undo", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    const auto emptyPool = fixture.xml();

    CHECK (commands::removeUnusedClips (fixture.project, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (emptyPool));

    const auto clipId = addedEventClip (fixture, "Riff");
    placeClip (fixture, 0, clipId, 0);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (commands::removeUnusedClips (fixture.project, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removeUnusedClips leaves a child of the pool that is no Clip where it is", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    addedEventClip (fixture, "unused");

    // What a newer build might have left in the pool. The schema name of
    // another node stands in for a type this build does not know.
    auto clipPool = fixture.project.getChildWithName (ids::ClipPool);
    clipPool.addChild (juce::ValueTree (ids::Marker), 0, nullptr);

    REQUIRE (commands::removeUnusedClips (fixture.project, fixture.undoManager).wasApplied());

    CHECK (clipsOf (fixture).empty());
    REQUIRE (clipPool.getNumChildren() == 1);
    CHECK (clipPool.getChild (0).hasType (ids::Marker));
}

TEST_CASE ("A clip command for a clip that is not there is refused and changes nothing", "[model][commands][ClipPoolCommands]")
{
    CommandFixture fixture;
    addedEventClip (fixture, "Lead");
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    const auto results = {
        renameInPool (fixture, unknownId, "x"),
        colourInPool (fixture, unknownId, ""),
        commands::removeClip (fixture.project, unknownId, fixture.undoManager),
        commands::removeClip (fixture.project, "", fixture.undoManager),
        // A Track's id names no Clip.
        commands::removeClip (fixture.project, "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b", fixture.undoManager),
        renameInPool (fixture, CommandFixture::minimoogId, "x"),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
        CHECK (result.createdId.isEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}
