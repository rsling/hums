// model::commands for placements (docs/SCHEMA.md -> Placements / Placement,
// docs/EDITING.md -> Split, Resize).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/SlotCommands.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";
const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

juce::String idOfTrack (const CommandFixture& fixture, int position)
{
    return model::TrackModel (fixture.track (position)).getId();
}

juce::String addedEventClip (CommandFixture& fixture, std::int64_t loopLength)
{
    const auto result = commands::addEventClip (fixture.project, "Riff", loopLength, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

juce::String placed (CommandFixture& fixture, int trackPosition, const juce::String& clipId, std::int64_t start, std::int64_t length)
{
    const auto result = commands::addPlacement (fixture.project, idOfTrack (fixture, trackPosition), clipId, start, length, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

// A track's own placements in document order, each as "start+length" and,
// where one is stored, "@offset".
std::string placementsOf (const CommandFixture& fixture, int trackPosition)
{
    std::string text;

    for (const auto& placement : model::TrackModel (fixture.track (trackPosition)).getPlacements())
    {
        text += std::to_string (placement.getStart()) + "+" + std::to_string (placement.getLength());

        if (placement.getOffset() != 0)
            text += "@" + std::to_string (placement.getOffset());

        text += " ";
    }

    return text;
}

model::PlacementModel placementAt (const CommandFixture& fixture, int trackPosition, int index)
{
    return model::TrackModel (fixture.track (trackPosition)).getPlacements()[static_cast<std::size_t> (index)];
}

int refcountOf (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getRefcount (clipId);
}

// A placement on a new automation lane of the track, put into the tree as a
// loaded file would hold it: the commands for lanes are not what is tested
// here. An empty event clip holds no note, so a lane may reference it.
juce::String placedOnLane (CommandFixture& fixture, int trackPosition, const juce::String& clipId, std::int64_t start, std::int64_t length)
{
    auto lanes = fixture.track (trackPosition).getChildWithName (ids::AutomationLanes);

    if (lanes.getNumChildren() == 0)
    {
        juce::ValueTree lane (ids::AutomationLane);
        lane.setProperty (ids::id, juce::Uuid().toString(), nullptr);
        lanes.appendChild (lane, nullptr);
    }

    const auto id = juce::Uuid().toString();

    juce::ValueTree placement (ids::Placement);
    placement.setProperty (ids::id, id, nullptr);
    placement.setProperty (ids::clipId, clipId, nullptr);
    placement.setProperty (ids::start, static_cast<juce::int64> (start), nullptr);
    placement.setProperty (ids::length, static_cast<juce::int64> (length), nullptr);
    lanes.getChild (0).appendChild (placement, nullptr);

    return id;
}

// The placements of the track's first automation lane, as placementsOf.
std::string lanePlacementsOf (const CommandFixture& fixture, int trackPosition)
{
    std::string text;

    for (const auto& placement : model::TrackModel (fixture.track (trackPosition)).getAutomationLanes()[0].getPlacements())
    {
        text += std::to_string (placement.getStart()) + "+" + std::to_string (placement.getLength());

        if (placement.getOffset() != 0)
            text += "@" + std::to_string (placement.getOffset());

        text += " ";
    }

    return text;
}

// A new automation lane of the track, made by its command; the id.
juce::String addedLane (CommandFixture& fixture, int trackPosition)
{
    const auto result = commands::addAutomationLane (fixture.project, idOfTrack (fixture, trackPosition), "Cutoff", fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

// Content, put into the tree as a loaded file would hold it: the commands
// for it are not what is tested here.
juce::ValueTree eventsOf (const CommandFixture& fixture, const juce::String& clipId)
{
    return fixture.project.getChildWithName (ids::ClipPool).getChildWithProperty (ids::id, clipId).getChildWithName (ids::Events);
}

void addEvent (CommandFixture& fixture, const juce::String& clipId, const juce::String& kind, std::int64_t tick)
{
    juce::ValueTree event (ids::Event);
    event.setProperty (ids::tick, static_cast<juce::int64> (tick), nullptr);
    event.setProperty (ids::kind, kind, nullptr);
    eventsOf (fixture, clipId).appendChild (event, nullptr);
}

void addNote (CommandFixture& fixture, const juce::String& clipId, std::int64_t tick)
{
    juce::ValueTree note (ids::Note);
    note.setProperty (ids::tick, static_cast<juce::int64> (tick), nullptr);
    note.setProperty (ids::length, 240, nullptr);
    note.setProperty (ids::pitch, 60, nullptr);
    eventsOf (fixture, clipId).appendChild (note, nullptr);
}

void checkRefused (const commands::CommandResult& result)
{
    CHECK (! result.wasApplied());
    CHECK (result.refusal.isNotEmpty());
    CHECK (result.createdId.isEmpty());
}

} // namespace

TEST_CASE ("addPlacement puts a clip on a track from the beginning of its loop, and undo takes it away again", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addPlacement (fixture.project, idOfTrack (fixture, 1), clipId, 7680, 5000, fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0).empty());
    REQUIRE (placementsOf (fixture, 1) == "7680+5000 ");

    const auto added = placementAt (fixture, 1, 0);
    CHECK (added.getId() == result.createdId);
    CHECK (added.getClipId() == clipId);
    CHECK (added.getOffset() == 0);
    CHECK (refcountOf (fixture, clipId) == 1);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (refcountOf (fixture, clipId) == 0);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("A clip of either kind is placed any number of times: next to itself, and at one time on several tracks", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto eventClipId = addedEventClip (fixture, 3840);
    const auto stepClip = commands::addStepClip (fixture.project, "Bass", fixture.undoManager);
    REQUIRE (stepClip.wasApplied());

    // One begins on the tick at which another ends, before and behind.
    const auto middle = placed (fixture, 0, eventClipId, 1000, 1000);
    const auto behind = placed (fixture, 0, eventClipId, 2000, 1);
    const auto before = placed (fixture, 0, stepClip.createdId, 0, 1000);
    placed (fixture, 1, eventClipId, 1000, 1000);
    placed (fixture, 2, stepClip.createdId, 1000, 1000);

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "1000+1000 2000+1 0+1000 ");
    CHECK (refcountOf (fixture, eventClipId) == 3);
    CHECK (refcountOf (fixture, stepClip.createdId) == 2);
    CHECK (middle != behind);
    CHECK (behind != before);
}

TEST_CASE ("A placement may end on the largest tick there is, and not past it", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto trackId = idOfTrack (fixture, 0);

    checkRefused (commands::addPlacement (fixture.project, trackId, clipId, largestTick, 1, fixture.undoManager));
    checkRefused (commands::addPlacement (fixture.project, trackId, clipId, 1, largestTick, fixture.undoManager));
    checkRefused (commands::addPlacement (fixture.project, trackId, clipId, largestTick, largestTick, fixture.undoManager));
    CHECK (placementsOf (fixture, 0).empty());

    const auto id = placed (fixture, 0, clipId, 0, largestTick);
    CHECK (fixture.isValid());

    checkRefused (commands::movePlacement (fixture.project, id, 1, fixture.undoManager));
    checkRefused (commands::addPlacement (fixture.project, trackId, clipId, largestTick - 1, 1, fixture.undoManager));

    REQUIRE (commands::resizePlacement (fixture.project, id, largestTick - 10, fixture.undoManager).wasApplied());
    REQUIRE (commands::movePlacement (fixture.project, id, 10, fixture.undoManager).wasApplied());
    checkRefused (commands::resizePlacement (fixture.project, id, largestTick - 9, fixture.undoManager));
    checkRefused (commands::movePlacement (fixture.project, id, 11, fixture.undoManager));

    // Split far out: (0 + (largest - 20 - 10)) mod 3840.
    const auto split = commands::splitPlacement (fixture.project, id, largestTick - 20, fixture.undoManager);
    REQUIRE (split.wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementAt (fixture, 0, 0).getLength() == largestTick - 30);
    CHECK (placementAt (fixture, 0, 1).getStart() == largestTick - 20);
    CHECK (placementAt (fixture, 0, 1).getLength() == 20);
    CHECK (placementAt (fixture, 0, 1).getOffset() == (largestTick - 30) % 3840);
}

TEST_CASE ("addPlacement refuses a track or a clip that is not there, and a start or a length a placement cannot have", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto trackId = idOfTrack (fixture, 0);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    const auto results = {
        commands::addPlacement (fixture.project, unknownId, clipId, 0, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, "", clipId, 0, 960, fixture.undoManager),
        // A clip's id names no track, and a track's id no clip.
        commands::addPlacement (fixture.project, clipId, clipId, 0, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, trackId, 0, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, unknownId, 0, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, "", 0, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, clipId, -1, 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, clipId, std::numeric_limits<std::int64_t>::min(), 960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, clipId, 0, 0, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, clipId, 0, -960, fixture.undoManager),
        commands::addPlacement (fixture.project, trackId, clipId, 0, std::numeric_limits<std::int64_t>::min(), fixture.undoManager),
    };

    for (const auto& result : results)
        checkRefused (result);

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("addPlacement refuses a placement that would share a tick with another one of the track", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto otherClipId = addedEventClip (fixture, 960);
    const auto trackId = idOfTrack (fixture, 0);
    placed (fixture, 0, clipId, 1000, 1000);
    placed (fixture, 0, clipId, 3000, 1000);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    struct Extent
    {
        std::int64_t start;
        std::int64_t length;
    };

    const std::vector<Extent> inTheWay {
        { 1000, 1000 }, // the same ticks
        { 1200, 100 },  // inside
        { 0, 5000 },    // around both
        { 500, 501 },   // over the start by one tick
        { 1999, 500 },  // over the end by one tick
        { 1999, 1 },    // its last tick
        { 1000, 1 },    // its first tick
        { 2000, 1001 }, // between the two, one tick too long
        { 3999, largestTick - 3999 },
    };

    for (const auto& extent : inTheWay)
    {
        INFO (extent.start << " + " << extent.length);
        checkRefused (commands::addPlacement (fixture.project, trackId, clipId, extent.start, extent.length, fixture.undoManager));

        // Whichever clip: the track is taken, not the clip.
        checkRefused (commands::addPlacement (fixture.project, trackId, otherClipId, extent.start, extent.length, fixture.undoManager));
    }

    CHECK (fixture.isUntouchedSince (before));

    // The gap between the two takes exactly its length.
    CHECK (commands::addPlacement (fixture.project, trackId, otherClipId, 2000, 1000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("movePlacement moves a placement along its track and keeps its length and offset; one undo takes it back", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto id = placed (fixture, 0, clipId, 0, 4000);
    const auto split = commands::splitPlacement (fixture.project, id, 1000, fixture.undoManager);
    REQUIRE (split.wasApplied());
    REQUIRE (placementsOf (fixture, 0) == "0+1000 1000+3000@1000 ");
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::movePlacement (fixture.project, split.createdId, 9600, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 9600+3000@1000 ");
    CHECK (placementAt (fixture, 0, 1).getId() == split.createdId);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("A placement is not in its own way: it can be moved by less than its length, and up to its neighbours", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    placed (fixture, 0, clipId, 0, 1000);
    const auto id = placed (fixture, 0, clipId, 2000, 1000);
    placed (fixture, 0, clipId, 4000, 1000);

    REQUIRE (commands::movePlacement (fixture.project, id, 2500, fixture.undoManager).wasApplied());
    REQUIRE (commands::movePlacement (fixture.project, id, 3000, fixture.undoManager).wasApplied());
    REQUIRE (commands::movePlacement (fixture.project, id, 1000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 1000+1000 4000+1000 ");

    // Over the others, to the far side.
    REQUIRE (commands::movePlacement (fixture.project, id, 5000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 5000+1000 4000+1000 ");
}

TEST_CASE ("movePlacement refuses a negative start and a place that is taken", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    placed (fixture, 0, clipId, 0, 1000);
    const auto id = placed (fixture, 0, clipId, 2000, 1000);
    placed (fixture, 0, clipId, 4000, 1000);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    for (const auto toStart : { std::int64_t { -1 }, std::numeric_limits<std::int64_t>::min(), std::int64_t { 0 }, std::int64_t { 999 }, std::int64_t { 3001 }, std::int64_t { 4999 }, largestTick })
    {
        INFO (toStart);
        checkRefused (commands::movePlacement (fixture.project, id, toStart, fixture.undoManager));
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("Moving a placement to where it is, or giving it the length it has, is applied and leaves nothing to undo", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 2000, 1000);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    CHECK (commands::movePlacement (fixture.project, id, 2000, fixture.undoManager).wasApplied());
    CHECK (commands::movePlacementToTrack (fixture.project, id, idOfTrack (fixture, 0), 2000, fixture.undoManager).wasApplied());
    CHECK (commands::resizePlacement (fixture.project, id, 1000, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("movePlacementToTrack takes a placement to another track as the same placement, in one undo step", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto first = placed (fixture, 0, clipId, 0, 4000);
    const auto split = commands::splitPlacement (fixture.project, first, 1000, fixture.undoManager);
    REQUIRE (split.wasApplied());
    placed (fixture, 2, clipId, 0, 500);
    const auto before = fixture.xml();

    // A handle to the node, as a selection holds one.
    const auto handle = fixture.track (0).getChildWithName (ids::Placements).getChild (1);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::movePlacementToTrack (fixture.project, split.createdId, idOfTrack (fixture, 2), 500, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 ");
    CHECK (placementsOf (fixture, 2) == "0+500 500+3000@1000 ");
    CHECK (placementAt (fixture, 2, 1).getId() == split.createdId);
    CHECK (placementAt (fixture, 2, 1).getClipId() == clipId);
    CHECK (handle.getParent() == fixture.track (2).getChildWithName (ids::Placements));
    CHECK (refcountOf (fixture, clipId) == 3);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (handle.getParent() == fixture.track (0).getChildWithName (ids::Placements));
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("movePlacementToTrack to the placement's own track moves it along that track", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 0, 1000);
    placed (fixture, 0, clipId, 2000, 1000);

    // By less than its length: it is not in its own way here either.
    REQUIRE (commands::movePlacementToTrack (fixture.project, id, idOfTrack (fixture, 0), 500, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "500+1000 2000+1000 ");

    checkRefused (commands::movePlacementToTrack (fixture.project, id, idOfTrack (fixture, 0), 1001, fixture.undoManager));
}

TEST_CASE ("movePlacementToTrack refuses a track that is not there, a start that cannot be, and a place that is taken", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 0, 1000);
    placed (fixture, 1, clipId, 500, 1000);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();
    const auto otherTrackId = idOfTrack (fixture, 1);

    const auto results = {
        commands::movePlacementToTrack (fixture.project, id, unknownId, 0, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, id, "", 0, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, id, clipId, 0, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, id, otherTrackId, -1, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, id, otherTrackId, largestTick, fixture.undoManager),
        // Free on its own track, taken on the other.
        commands::movePlacementToTrack (fixture.project, id, otherTrackId, 0, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, id, otherTrackId, 1499, fixture.undoManager),
    };

    for (const auto& result : results)
        checkRefused (result);

    CHECK (fixture.isUntouchedSince (before));

    CHECK (commands::movePlacementToTrack (fixture.project, id, otherTrackId, 1500, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("resizePlacement repeats more of the loop or plays less of it, and leaves start and offset alone", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto first = placed (fixture, 0, clipId, 0, 4000);
    const auto split = commands::splitPlacement (fixture.project, first, 1000, fixture.undoManager);
    REQUIRE (split.wasApplied());
    const auto before = fixture.xml();

    // Longer than the loop many times over, then shorter than one pass.
    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::resizePlacement (fixture.project, split.createdId, 120000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 1000+120000@1000 ");

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);

    REQUIRE (commands::resizePlacement (fixture.project, split.createdId, 1, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 1000+1@1000 ");

    // The clip is as it was: resize is no stretch.
    CHECK (model::ClipModel (fixture.project.getChildWithName (ids::ClipPool).getChild (0)).getLoopLength() == 1200);
}

TEST_CASE ("resizePlacement refuses a length that is none, and one that reaches into the next placement", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 1000, 1000);
    placed (fixture, 0, clipId, 3000, 1000);
    placed (fixture, 0, clipId, 0, 500);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    for (const auto length : { std::int64_t { 0 }, std::int64_t { -1 }, std::numeric_limits<std::int64_t>::min(), std::int64_t { 2001 }, std::int64_t { 50000 }, largestTick })
    {
        INFO (length);
        checkRefused (commands::resizePlacement (fixture.project, id, length, fixture.undoManager));
    }

    CHECK (fixture.isUntouchedSince (before));

    // Up to the next one, exactly.
    CHECK (commands::resizePlacement (fixture.project, id, 2000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("splitPlacement makes two placements of the same clip; the second goes on where the first stops in the loop", "[model][commands][PlacementCommands]")
{
    // The example of docs/SCHEMA.md: a loop of 1200 ticks, split after
    // 7680 ticks, where its phase is 7680 mod 1200 = 480.
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto otherClipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 0, 15360);
    placed (fixture, 0, otherClipId, 20000, 100);
    const auto before = fixture.xml();
    const auto poolBefore = fixture.project.getChildWithName (ids::ClipPool).toXmlString();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::splitPlacement (fixture.project, id, 7680, fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    // The new one stands right behind the one it was split from.
    REQUIRE (placementsOf (fixture, 0) == "0+7680 7680+7680@480 20000+100 ");
    CHECK (placementAt (fixture, 0, 0).getId() == id);
    CHECK (placementAt (fixture, 0, 1).getId() == result.createdId);
    CHECK (result.createdId != id);
    CHECK (placementAt (fixture, 0, 1).getClipId() == clipId);
    CHECK (refcountOf (fixture, clipId) == 2);
    CHECK (fixture.project.getChildWithName (ids::ClipPool).toXmlString() == poolBefore);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("splitPlacement counts the offset the placement already has, and from the placement's start", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto id = placed (fixture, 0, clipId, 5000, 10000);

    // (0 + (5700 - 5000)) mod 1200 = 700.
    const auto second = commands::splitPlacement (fixture.project, id, 5700, fixture.undoManager);
    REQUIRE (second.wasApplied());
    REQUIRE (placementsOf (fixture, 0) == "5000+700 5700+9300@700 ");

    // (700 + (6400 - 5700)) mod 1200 = 200: once around the loop.
    const auto third = commands::splitPlacement (fixture.project, second.createdId, 6400, fixture.undoManager);
    REQUIRE (third.wasApplied());
    REQUIRE (placementsOf (fixture, 0) == "5000+700 5700+700@700 6400+8600@200 ");

    // (200 + (7400 - 6400)) mod 1200 = 0: at the loop point no offset is
    // stored.
    const auto fourth = commands::splitPlacement (fixture.project, third.createdId, 7400, fixture.undoManager);
    REQUIRE (fourth.wasApplied());
    CHECK (placementsOf (fixture, 0) == "5000+700 5700+700@700 6400+1000@200 7400+7600 ");
    CHECK (! fixture.track (0).getChildWithName (ids::Placements).getChild (3).hasProperty (ids::offset));

    CHECK (fixture.isValid());
    CHECK (refcountOf (fixture, clipId) == 4);
}

TEST_CASE ("splitPlacement splits a placement of a step clip by the loop length that follows from its grid", "[model][commands][PlacementCommands]")
{
    // The default grid: 16 columns of 240 ticks, a loop of 3840.
    CommandFixture fixture;
    const auto stepClip = commands::addStepClip (fixture.project, "Bass", fixture.undoManager);
    REQUIRE (stepClip.wasApplied());
    const auto id = placed (fixture, 0, stepClip.createdId, 0, 10000);

    REQUIRE (commands::splitPlacement (fixture.project, id, 4000, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+4000 4000+6000@160 ");
}

TEST_CASE ("splitPlacement refuses a tick that is not inside the placement", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto id = placed (fixture, 0, clipId, 1000, 1000);
    const auto oneTick = placed (fixture, 0, clipId, 5000, 1);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    for (const auto atTick : { std::int64_t { 1000 }, std::int64_t { 2000 }, std::int64_t { 999 }, std::int64_t { 2001 }, std::int64_t { 0 }, std::int64_t { -1 }, std::numeric_limits<std::int64_t>::min(), largestTick })
    {
        INFO (atTick);
        checkRefused (commands::splitPlacement (fixture.project, id, atTick, fixture.undoManager));
    }

    // A placement of one tick has no tick inside.
    checkRefused (commands::splitPlacement (fixture.project, oneTick, 5000, fixture.undoManager));
    checkRefused (commands::splitPlacement (fixture.project, oneTick, 5001, fixture.undoManager));

    CHECK (fixture.isUntouchedSince (before));

    // The first and the last tick at which it can be split.
    CHECK (commands::splitPlacement (fixture.project, id, 1001, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("removePlacement takes the placement away and leaves its clip in the pool; undo puts it back where it was", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    placed (fixture, 0, clipId, 0, 1000);
    const auto id = placed (fixture, 0, clipId, 2000, 1000);
    placed (fixture, 0, clipId, 4000, 1000);
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removePlacement (fixture.project, id, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (placementsOf (fixture, 0) == "0+1000 4000+1000 ");
    CHECK (refcountOf (fixture, clipId) == 2);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (placementsOf (fixture, 0) == "0+1000 2000+1000 4000+1000 ");
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);

    // The last reference gone, the clip is still there.
    REQUIRE (commands::removePlacement (fixture.project, placementAt (fixture, 0, 0).getId(), fixture.undoManager).wasApplied());
    REQUIRE (commands::removePlacement (fixture.project, placementAt (fixture, 0, 0).getId(), fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (refcountOf (fixture, clipId) == 0);
    CHECK (fixture.project.getChildWithName (ids::ClipPool).getNumChildren() == 1);
}

TEST_CASE ("A placement on an automation lane is moved in time, resized, split and removed like a track's own", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto id = placedOnLane (fixture, 0, clipId, 0, 1000);
    placedOnLane (fixture, 0, clipId, 5000, 1000);

    // At the same ticks as the lane's, on the track itself: another parent.
    placed (fixture, 0, addedEventClip (fixture, 1200), 0, 9000);
    REQUIRE (fixture.isValid());

    REQUIRE (commands::movePlacement (fixture.project, id, 1000, fixture.undoManager).wasApplied());
    REQUIRE (commands::resizePlacement (fixture.project, id, 4000, fixture.undoManager).wasApplied());
    CHECK (lanePlacementsOf (fixture, 0) == "1000+4000 5000+1000 ");

    const auto split = commands::splitPlacement (fixture.project, id, 3000, fixture.undoManager);
    REQUIRE (split.wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanePlacementsOf (fixture, 0) == "1000+2000 3000+2000@800 5000+1000 ");

    REQUIRE (commands::removePlacement (fixture.project, id, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanePlacementsOf (fixture, 0) == "3000+2000@800 5000+1000 ");
    CHECK (placementsOf (fixture, 0) == "0+9000 ");
}

TEST_CASE ("On an automation lane too, a placement gives way to no other of its lane", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto id = placedOnLane (fixture, 0, clipId, 0, 1000);
    placedOnLane (fixture, 0, clipId, 5000, 1000);
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    checkRefused (commands::movePlacement (fixture.project, id, 4001, fixture.undoManager));
    checkRefused (commands::resizePlacement (fixture.project, id, 5001, fixture.undoManager));

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("movePlacementToTrack refuses a placement that is on an automation lane", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 1200);
    const auto id = placedOnLane (fixture, 0, clipId, 0, 1000);
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    checkRefused (commands::movePlacementToTrack (fixture.project, id, idOfTrack (fixture, 0), 0, fixture.undoManager));
    checkRefused (commands::movePlacementToTrack (fixture.project, id, idOfTrack (fixture, 1), 0, fixture.undoManager));

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("addLanePlacement puts a cc-only clip on an automation lane from the beginning of its loop, and undo takes it away again", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    addEvent (fixture, clipId, "cc", 0);
    addEvent (fixture, clipId, "cc", 960);
    const auto laneId = addedLane (fixture, 0);

    // On the track itself, at the same ticks: another parent, not in the way.
    placed (fixture, 0, addedEventClip (fixture, 3840), 0, 9000);
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addLanePlacement (fixture.project, laneId, clipId, 1000, 5000, fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());
    CHECK (lanePlacementsOf (fixture, 0) == "1000+5000 ");
    CHECK (placementsOf (fixture, 0) == "0+9000 ");
    CHECK (refcountOf (fixture, clipId) == 1);

    const auto placement = model::TrackModel (fixture.track (0)).getAutomationLanes()[0].getPlacements()[0];
    CHECK (placement.getId() == result.createdId);
    CHECK (placement.getClipId() == clipId);
    CHECK (placement.getOffset() == 0);

    // Right behind it, on the tick at which it ends.
    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::addLanePlacement (fixture.project, laneId, clipId, 6000, 1, fixture.undoManager).wasApplied());
    CHECK (lanePlacementsOf (fixture, 0) == "1000+5000 6000+1 ");

    REQUIRE (fixture.undoManager.undo());
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A clip on an automation lane stays on lanes: no track takes it, and no lane takes a clip of a track or a slot", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto ofALane = addedEventClip (fixture, 3840);
    const auto ofATrack = addedEventClip (fixture, 3840);
    const auto ofASlot = addedEventClip (fixture, 3840);
    const auto laneId = addedLane (fixture, 0);
    const auto otherLaneId = addedLane (fixture, 1);

    REQUIRE (commands::addLanePlacement (fixture.project, laneId, ofALane, 0, 3840, fixture.undoManager).wasApplied());
    placed (fixture, 0, ofATrack, 0, 3840);
    REQUIRE (commands::setSlot (fixture.project, idOfTrack (fixture, 1), 0, ofASlot, fixture.undoManager).wasApplied());
    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    // Each of the three is an empty event clip, so its content is not what
    // is in the way.
    checkRefused (commands::addPlacement (fixture.project, idOfTrack (fixture, 0), ofALane, 20000, 3840, fixture.undoManager));
    checkRefused (commands::addPlacement (fixture.project, idOfTrack (fixture, 1), ofALane, 20000, 3840, fixture.undoManager));
    checkRefused (commands::addLanePlacement (fixture.project, laneId, ofATrack, 20000, 3840, fixture.undoManager));
    checkRefused (commands::addLanePlacement (fixture.project, otherLaneId, ofASlot, 20000, 3840, fixture.undoManager));
    CHECK (fixture.isUntouchedSince (before));

    // A lane's clip goes on any lane, any number of times.
    CHECK (commands::addLanePlacement (fixture.project, otherLaneId, ofALane, 0, 3840, fixture.undoManager).wasApplied());

    // Once nothing of the other side references a clip, it may change sides.
    for (const auto& placement : model::TrackModel (fixture.track (0)).getPlacements())
        REQUIRE (commands::removePlacement (fixture.project, placement.getId(), fixture.undoManager).wasApplied());

    CHECK (commands::addLanePlacement (fixture.project, laneId, ofATrack, 20000, 3840, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("addLanePlacement takes an empty event clip", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto laneId = addedLane (fixture, 0);

    REQUIRE (commands::addLanePlacement (fixture.project, laneId, clipId, 0, 3840, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("addLanePlacement refuses a clip that holds anything but cc events: invariant 9 from the side of the lane", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto laneId = addedLane (fixture, 0);

    const auto withNote = addedEventClip (fixture, 3840);
    addEvent (fixture, withNote, "cc", 0);
    addNote (fixture, withNote, 0);

    const auto withBend = addedEventClip (fixture, 3840);
    addEvent (fixture, withBend, "cc", 0);
    addEvent (fixture, withBend, "pitchBend", 0);

    const auto stepClip = commands::addStepClip (fixture.project, "Bass", fixture.undoManager);
    REQUIRE (stepClip.wasApplied());

    REQUIRE (fixture.isValid());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    checkRefused (commands::addLanePlacement (fixture.project, laneId, withNote, 0, 3840, fixture.undoManager));
    checkRefused (commands::addLanePlacement (fixture.project, laneId, withBend, 0, 3840, fixture.undoManager));
    checkRefused (commands::addLanePlacement (fixture.project, laneId, stepClip.createdId, 0, 3840, fixture.undoManager));

    CHECK (fixture.isUntouchedSince (before));

    // The same clips go on the track itself.
    CHECK (commands::addPlacement (fixture.project, idOfTrack (fixture, 0), withNote, 0, 3840, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("addLanePlacement refuses a lane or a clip that is not there, an extent a placement cannot have, and a place that is taken on the lane", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    const auto laneId = addedLane (fixture, 0);
    const auto otherLaneId = addedLane (fixture, 0);
    REQUIRE (commands::addLanePlacement (fixture.project, laneId, clipId, 1000, 1000, fixture.undoManager).wasApplied());
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    const auto results = {
        commands::addLanePlacement (fixture.project, unknownId, clipId, 0, 100, fixture.undoManager),
        // The id of the lane's track names no lane.
        commands::addLanePlacement (fixture.project, idOfTrack (fixture, 0), clipId, 0, 100, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, unknownId, 0, 100, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, clipId, -1, 100, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, clipId, 0, 0, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, clipId, 2, largestTick, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, clipId, 0, 1001, fixture.undoManager),
        commands::addLanePlacement (fixture.project, laneId, clipId, 1999, 1, fixture.undoManager),
    };

    for (const auto& result : results)
        checkRefused (result);

    CHECK (fixture.isUntouchedSince (before));

    // Another lane of the same track is another parent.
    CHECK (commands::addLanePlacement (fixture.project, otherLaneId, clipId, 1000, 1000, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("A placement command for a placement that is not there is refused and changes nothing", "[model][commands][PlacementCommands]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, 3840);
    placed (fixture, 0, clipId, 0, 1000);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();
    const auto trackId = idOfTrack (fixture, 0);

    const auto results = {
        commands::movePlacement (fixture.project, unknownId, 5000, fixture.undoManager),
        commands::movePlacementToTrack (fixture.project, unknownId, trackId, 5000, fixture.undoManager),
        commands::resizePlacement (fixture.project, unknownId, 500, fixture.undoManager),
        commands::splitPlacement (fixture.project, unknownId, 500, fixture.undoManager),
        commands::removePlacement (fixture.project, unknownId, fixture.undoManager),
        commands::removePlacement (fixture.project, "", fixture.undoManager),
        // The ids of the clip and of the track name no placement.
        commands::removePlacement (fixture.project, clipId, fixture.undoManager),
        commands::movePlacement (fixture.project, trackId, 5000, fixture.undoManager),
        commands::splitPlacement (fixture.project, clipId, 500, fixture.undoManager),
    };

    for (const auto& result : results)
        checkRefused (result);

    CHECK (fixture.isUntouchedSince (before));
}
