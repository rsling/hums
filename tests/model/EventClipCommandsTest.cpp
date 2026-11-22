// model::commands for what an event clip holds: notes, events, the loop
// length and the quantise setting (docs/SCHEMA.md -> ClipPool / Clip,
// -> Event clip: Events; docs/EDITING.md).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/ClipPoolModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/EventClipCommands.h"
#include "model/commands/NodeLookup.h"
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
using commands::EditContext;
using commands::EventValues;
using commands::NoteValues;
using Kind = model::EventModel::Kind;
using Policy = model::Settings::SharedEditPolicy;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";
const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

// The command fixture with what these tests need on top: clips, and
// references to them, each made through its own command.
struct Song : CommandFixture
{
    juce::String trackId (int position) const
    {
        return model::TrackModel (track (position)).getId();
    }

    // An event clip with a loop of one bar.
    juce::String eventClip()
    {
        const auto result = commands::addEventClip (project, "Clip", 3840, undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    juce::String stepClip()
    {
        const auto result = commands::addStepClip (project, "Steps", undoManager);
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

    // A new automation lane on that track with a placement of one bar of
    // the clip; the result is the placement's id.
    juce::String placeOnALane (int trackPosition, const juce::String& clipId, std::int64_t start)
    {
        const auto lane = commands::addAutomationLane (project, trackId (trackPosition), "Lane", undoManager);
        REQUIRE (lane.wasApplied());
        const auto result = commands::addLanePlacement (project, lane.createdId, clipId, start, 3840, undoManager);
        REQUIRE (result.wasApplied());
        return result.createdId;
    }

    model::ClipModel clip (const juce::String& clipId) const
    {
        return model::ClipModel (commands::NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId));
    }

    int clipCount() const
    {
        return static_cast<int> (model::ClipPoolModel (project.getChildWithName (ids::ClipPool)).getClips().size());
    }

    model::PlacementModel placement (const juce::String& placementId) const
    {
        return model::PlacementModel (commands::NodeLookup::placementWithId (project, placementId));
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
};

NoteValues note (std::int64_t tick, std::int64_t length, int pitch)
{
    NoteValues values;
    values.tick = tick;
    values.length = length;
    values.pitch = pitch;
    return values;
}

EventValues event (std::int64_t tick, Kind kind, int a, int b)
{
    EventValues values;
    values.tick = tick;
    values.kind = kind;
    values.a = a;
    values.b = b;
    return values;
}

EventValues sysex (const juce::String& data)
{
    EventValues values;
    values.kind = Kind::sysex;
    values.data = data;
    return values;
}

// The pitches of a clip's notes in child order, as one text.
std::string pitchesOf (const model::ClipModel& clip)
{
    std::string pitches;

    for (const auto& stored : clip.getNotes())
        pitches += std::to_string (stored.getPitch()) + " ";

    return pitches;
}

// The a of a clip's events in child order, as one text.
std::string controllersOf (const model::ClipModel& clip)
{
    std::string controllers;

    for (const auto& stored : clip.getEvents())
        controllers += std::to_string (stored.getA()) + " ";

    return controllers;
}

} // namespace

//==============================================================================
// Notes.

TEST_CASE ("addNote puts a note with its values behind the others, and undo takes it away again", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();

    NoteValues values;
    values.tick = 480;
    values.length = 120;
    values.pitch = 72;
    values.velocity = 1;
    values.offVelocity = 0;
    values.enabled = false;

    EditContext poolLevel (Policy::updateAll);
    const auto result = commands::addNote (song.project, clipId, values, poolLevel, song.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (result.createdId.isEmpty());
    CHECK (song.isValid());

    const auto notes = song.clip (clipId).getNotes();
    REQUIRE (notes.size() == 2);
    CHECK (notes[0].getPitch() == 60);
    CHECK (notes[1].getTick() == 480);
    CHECK (notes[1].getLength() == 120);
    CHECK (notes[1].getPitch() == 72);
    CHECK (notes[1].getVelocity() == 1);
    CHECK (notes[1].getOffVelocity() == 0);
    CHECK (! notes[1].isEnabled());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("A note made of nothing but place and pitch is the note the schema defaults", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext poolLevel (Policy::updateAll);

    REQUIRE (commands::addNote (song.project, clipId, note (0, 1, 0), poolLevel, song.undoManager).wasApplied());

    const auto notes = song.clip (clipId).getNotes();
    REQUIRE (notes.size() == 1);
    CHECK (notes[0].getVelocity() == 100);
    CHECK (notes[0].getOffVelocity() == 64);
    CHECK (notes[0].isEnabled());
}

TEST_CASE ("A note may lie past the loop and reach the largest tick, and no further", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext poolLevel (Policy::updateAll);

    CHECK (commands::addNote (song.project, clipId, note (3840, 240, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (commands::addNote (song.project, clipId, note (100000, 240, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (commands::addNote (song.project, clipId, note (largestTick - 10, 10, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (song.isValid());

    CHECK (! commands::addNote (song.project, clipId, note (largestTick - 10, 11, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (! commands::addNote (song.project, clipId, note (1, largestTick, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (song.clip (clipId).getNotes().size() == 3);
}

TEST_CASE ("addNote and setNote refuse values that are not a note's and change nothing", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    std::vector<NoteValues> wrong;
    wrong.push_back (note (-1, 240, 60));
    wrong.push_back (note (0, 0, 60));
    wrong.push_back (note (0, -240, 60));
    wrong.push_back (note (0, 240, -1));
    wrong.push_back (note (0, 240, 128));

    for (const auto velocity : { 0, 128, -1 })
    {
        wrong.push_back (note (0, 240, 60));
        wrong.back().velocity = velocity;
    }

    for (const auto offVelocity : { -1, 128 })
    {
        wrong.push_back (note (0, 240, 60));
        wrong.back().offVelocity = offVelocity;
    }

    for (const auto& values : wrong)
    {
        EditContext poolLevel (Policy::updateAll);
        const auto added = commands::addNote (song.project, clipId, values, poolLevel, song.undoManager);
        const auto set = commands::setNote (song.project, clipId, 0, values, poolLevel, song.undoManager);

        CHECK (! added.wasApplied());
        CHECK (added.refusal.isNotEmpty());
        CHECK (! set.wasApplied());
        CHECK (set.refusal.isNotEmpty());
    }

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("setNote gives a note new values and leaves it where it stands among the others", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);

    for (const auto pitch : { 60, 62, 64 })
        REQUIRE (commands::addNote (song.project, clipId, note (0, 240, pitch), first, song.undoManager).wasApplied());

    const auto before = song.startGesture();

    NoteValues values = note (960, 480, 63);
    values.velocity = 90;
    values.offVelocity = 10;
    values.enabled = false;

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::setNote (song.project, clipId, 1, values, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (pitchesOf (song.clip (clipId)) == "60 63 64 ");

    const auto changed = song.clip (clipId).getNotes()[1];
    CHECK (changed.getTick() == 960);
    CHECK (changed.getLength() == 480);
    CHECK (changed.getVelocity() == 90);
    CHECK (changed.getOffVelocity() == 10);
    CHECK (! changed.isEnabled());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("setNote with the values the note has is applied, leaves nothing to undo and makes no clone", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto placementId = song.place (0, clipId, 0);
    song.place (1, clipId, 0);

    // A note as a loaded file holds it, with its defaults not stored.
    juce::ValueTree stored (ids::Note);
    stored.setProperty (ids::tick, "240", nullptr);
    stored.setProperty (ids::length, "120", nullptr);
    stored.setProperty (ids::pitch, "48", nullptr);
    song.project.getChildWithName (ids::ClipPool).getChild (0).getChildWithName (ids::Events).appendChild (stored, nullptr);

    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    EditContext throughOne (Policy::makeUnique);
    throughOne.addPlacement (placementId);

    CHECK (commands::setNote (song.project, clipId, 0, note (240, 120, 48), throughOne, song.undoManager).wasApplied());
    CHECK (song.isUntouchedSince (before));
    CHECK (! throughOne.hasClone());
}

TEST_CASE ("setNote stores only what differs", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();

    juce::ValueTree stored (ids::Note);
    stored.setProperty (ids::tick, "240", nullptr);
    stored.setProperty (ids::length, "120", nullptr);
    stored.setProperty (ids::pitch, "48", nullptr);
    song.project.getChildWithName (ids::ClipPool).getChild (0).getChildWithName (ids::Events).appendChild (stored, nullptr);

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::setNote (song.project, clipId, 0, note (240, 120, 50), poolLevel, song.undoManager).wasApplied());

    CHECK (stored.getNumProperties() == 3);
    CHECK (model::NoteModel (stored).getPitch() == 50);
}

TEST_CASE ("removeNote takes out the note at that position and no other", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);

    // An event between the notes: positions count notes only.
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 62), first, song.undoManager).wasApplied());
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 64), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::removeNote (song.project, clipId, 1, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (pitchesOf (song.clip (clipId)) == "60 64 ");
    CHECK (song.clip (clipId).getEvents().size() == 1);

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (pitchesOf (song.clip (clipId)) == "60 62 64 ");
}

TEST_CASE ("A position at which the clip has no note or event is refused", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    for (const auto position : { -1, 1, 2, std::numeric_limits<int>::max(), std::numeric_limits<int>::min() })
    {
        EditContext poolLevel (Policy::updateAll);

        CHECK (! commands::setNote (song.project, clipId, position, note (0, 240, 61), poolLevel, song.undoManager).wasApplied());
        CHECK (! commands::removeNote (song.project, clipId, position, poolLevel, song.undoManager).wasApplied());
        CHECK (! commands::setEvent (song.project, clipId, position, event (0, Kind::cc, 75, 1), poolLevel, song.undoManager).wasApplied());
        CHECK (! commands::removeEvent (song.project, clipId, position, poolLevel, song.undoManager).wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

//==============================================================================
// Events.

TEST_CASE ("addEvent takes an event of every kind over the whole range of its values", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext poolLevel (Policy::updateAll);

    const std::vector<EventValues> right {
        event (0, Kind::cc, 0, 0),
        event (240, Kind::cc, 127, 127),
        event (0, Kind::pitchBend, 0, 0),
        event (0, Kind::pitchBend, 16383, 0),
        event (0, Kind::programChange, 127, 0),
        event (0, Kind::channelPressure, 127, 0),
        event (0, Kind::polyPressure, 127, 127),
        event (largestTick, Kind::cc, 1, 1),
        sysex (""),
        sysex ("7e7F0901"),
        sysex (juce::String::repeatedString ("0f", 64 * 1024)),
    };

    for (const auto& values : right)
        CHECK (commands::addEvent (song.project, clipId, values, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());

    const auto events = song.clip (clipId).getEvents();
    REQUIRE (events.size() == right.size());
    CHECK (events[1].getTick() == 240);
    CHECK (events[1].getKind() == Kind::cc);
    CHECK (events[1].getA() == 127);
    CHECK (events[1].getB() == 127);
    CHECK (events[1].isEnabled());
    CHECK (events[3].getKind() == Kind::pitchBend);
    CHECK (events[3].getA() == 16383);
    CHECK (events[9].getKind() == Kind::sysex);
    CHECK (events[9].getData() == "7e7F0901");
}

TEST_CASE ("addEvent and setEvent refuse values that are not those of an event of the kind", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    std::vector<EventValues> wrong {
        event (-1, Kind::cc, 74, 1),
        event (0, Kind::cc, 128, 1),
        event (0, Kind::cc, -1, 1),
        event (0, Kind::cc, 74, 128),
        event (0, Kind::cc, 74, -1),
        event (0, Kind::pitchBend, 16384, 0),
        event (0, Kind::pitchBend, -1, 0),
        event (0, Kind::pitchBend, 8192, 1),
        event (0, Kind::programChange, 128, 0),
        event (0, Kind::programChange, 1, 1),
        event (0, Kind::channelPressure, 128, 0),
        event (0, Kind::channelPressure, 1, 127),
        event (0, Kind::polyPressure, 128, 0),
        event (0, Kind::polyPressure, 60, 128),
        event (0, Kind::sysex, 1, 0),
        event (0, Kind::sysex, 0, 1),
        sysex ("7e7"),
        sysex ("7g"),
        sysex ("7e 7f"),
        sysex (juce::String::repeatedString ("0f", 64 * 1024 + 1)),
    };

    // Data on an event that is no sysex.
    wrong.push_back (event (0, Kind::cc, 74, 1));
    wrong.back().data = "7e";

    for (const auto& values : wrong)
    {
        EditContext poolLevel (Policy::updateAll);
        const auto added = commands::addEvent (song.project, clipId, values, poolLevel, song.undoManager);
        const auto set = commands::setEvent (song.project, clipId, 0, values, poolLevel, song.undoManager);

        INFO (model::EventModel::textOf (values.kind) << " a=" << values.a << " b=" << values.b << " tick=" << values.tick);
        CHECK (! added.wasApplied());
        CHECK (added.refusal.isNotEmpty());
        CHECK (! set.wasApplied());
    }

    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("setEvent gives an event new values, its kind included, and leaves it where it stands", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);

    // Bank select, then the program change: an order that must hold.
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 0, 1), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 32, 2), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::programChange, 5, 0), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();

    EditContext poolLevel (Policy::updateAll);
    auto values = sysex ("0102");
    values.tick = 120;
    values.enabled = false;
    REQUIRE (commands::setEvent (song.project, clipId, 1, values, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (controllersOf (song.clip (clipId)) == "0 0 5 ");

    const auto changed = song.clip (clipId).getEvents()[1];
    CHECK (changed.getTick() == 120);
    CHECK (changed.getKind() == Kind::sysex);
    CHECK (changed.getB() == 0);
    CHECK (changed.getData() == "0102");
    CHECK (! changed.isEnabled());

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);

    // And with the values it has: nothing more to undo.
    song.undoManager.beginNewTransaction();
    EditContext again (Policy::updateAll);
    CHECK (commands::setEvent (song.project, clipId, 1, values, again, song.undoManager).wasApplied());
    CHECK (song.undoManager.getNumActionsInCurrentTransaction() == 0);
    CHECK (song.xml() == after);
}

TEST_CASE ("removeEvent takes out the event at that position and no other", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);

    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 1, 1), first, song.undoManager).wasApplied());
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 2, 1), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 3, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::removeEvent (song.project, clipId, 1, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (controllersOf (song.clip (clipId)) == "1 3 ");
    CHECK (song.clip (clipId).getNotes().size() == 1);

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
}

//==============================================================================
// A clip on an automation lane holds nothing but cc events (invariant 9).

TEST_CASE ("A clip that an AutomationLane references takes cc events and nothing else", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    song.placeOnALane (0, clipId, 0);
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    EditContext poolLevel (Policy::updateAll);

    CHECK (! commands::addNote (song.project, clipId, note (0, 240, 60), poolLevel, song.undoManager).wasApplied());
    CHECK (! commands::addEvent (song.project, clipId, event (0, Kind::pitchBend, 8192, 0), poolLevel, song.undoManager).wasApplied());
    CHECK (! commands::addEvent (song.project, clipId, event (0, Kind::programChange, 1, 0), poolLevel, song.undoManager).wasApplied());
    CHECK (! commands::addEvent (song.project, clipId, sysex ("01"), poolLevel, song.undoManager).wasApplied());
    CHECK (! commands::setEvent (song.project, clipId, 0, event (0, Kind::channelPressure, 1, 0), poolLevel, song.undoManager).wasApplied());
    CHECK (song.isUntouchedSince (before));

    CHECK (commands::addEvent (song.project, clipId, event (240, Kind::cc, 74, 2), poolLevel, song.undoManager).wasApplied());
    CHECK (commands::setEvent (song.project, clipId, 0, event (0, Kind::cc, 71, 9), poolLevel, song.undoManager).wasApplied());
    CHECK (song.isValid());
}

TEST_CASE ("A clip on two lanes is refused a note through one lane's placement under update all", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto onLane = song.placeOnALane (0, clipId, 0);
    song.placeOnALane (1, clipId, 0);
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    EditContext throughLane (Policy::updateAll);
    throughLane.addPlacement (onLane);

    CHECK (! commands::addNote (song.project, clipId, note (0, 240, 60), throughLane, song.undoManager).wasApplied());
    CHECK (song.isUntouchedSince (before));
}

TEST_CASE ("Under make unique a note is refused through a lane's placement, and no clone is made", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto onLane = song.placeOnALane (0, clipId, 0);
    song.placeOnALane (1, clipId, 0);
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    EditContext throughLane (Policy::makeUnique);
    throughLane.addPlacement (onLane);

    CHECK (! commands::addNote (song.project, clipId, note (0, 240, 60), throughLane, song.undoManager).wasApplied());
    CHECK (! commands::addEvent (song.project, clipId, event (0, Kind::pitchBend, 1, 0), throughLane, song.undoManager).wasApplied());
    CHECK (song.isUntouchedSince (before));
    CHECK (! throughLane.hasClone());
    CHECK (song.clipCount() == 1);

    // A cc event goes into the lane's clone.
    CHECK (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), throughLane, song.undoManager).wasApplied());
    CHECK (throughLane.hasClone());
    CHECK (song.isValid());
}

TEST_CASE ("Once a gesture has cloned a lane's clip, the clone is held to the lane's rule as well", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto onLane = song.placeOnALane (0, clipId, 0);
    song.placeOnALane (1, clipId, 0);
    song.startGesture();

    EditContext throughLane (Policy::makeUnique);
    throughLane.addPlacement (onLane);
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), throughLane, song.undoManager).wasApplied());
    REQUIRE (throughLane.hasClone());

    CHECK (! commands::addNote (song.project, clipId, note (0, 240, 60), throughLane, song.undoManager).wasApplied());
    CHECK (! commands::setEvent (song.project, clipId, 0, event (0, Kind::programChange, 1, 0), throughLane, song.undoManager).wasApplied());
    CHECK (song.isValid());
}

//==============================================================================
// The gate.

TEST_CASE ("The commands of one gesture all change its one clone, each naming its content by position", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto edited = song.place (0, clipId, 0);
    const auto other = song.place (1, clipId, 0);
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    REQUIRE (commands::addNote (song.project, clipId, note (240, 240, 62), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();

    EditContext gesture (Policy::makeUnique);
    gesture.addPlacement (edited);

    REQUIRE (commands::setNote (song.project, clipId, 1, note (240, 240, 65), gesture, song.undoManager).wasApplied());
    REQUIRE (commands::addNote (song.project, clipId, note (480, 240, 67), gesture, song.undoManager).wasApplied());
    REQUIRE (commands::removeNote (song.project, clipId, 0, gesture, song.undoManager).wasApplied());
    REQUIRE (commands::setEvent (song.project, clipId, 0, event (0, Kind::cc, 71, 5), gesture, song.undoManager).wasApplied());
    REQUIRE (commands::removeEvent (song.project, clipId, 0, gesture, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::pitchBend, 1, 0), gesture, song.undoManager).wasApplied());
    REQUIRE (commands::setClipLoopLength (song.project, clipId, 960, gesture, song.undoManager).wasApplied());
    REQUIRE (commands::setClipQuantise (song.project, clipId, "1/8T", gesture, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (song.clipCount() == 2);
    REQUIRE (gesture.hasClone());

    const auto original = song.clip (clipId);
    CHECK (pitchesOf (original) == "60 62 ");
    CHECK (controllersOf (original) == "74 ");
    CHECK (original.getLoopLength() == 3840);
    CHECK (original.getQuantise().isEmpty());

    const auto clone = song.clip (gesture.getCloneId());
    CHECK (pitchesOf (clone) == "65 67 ");
    CHECK (controllersOf (clone) == "1 ");
    CHECK (clone.getLoopLength() == 960);
    CHECK (clone.getQuantise() == "1/8T");

    CHECK (song.placement (edited).getClipId() == gesture.getCloneId());
    CHECK (song.placement (other).getClipId() == clipId);

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
}

TEST_CASE ("Under update all a change through one placement is a change for every reference", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto edited = song.place (0, clipId, 0);
    song.place (1, clipId, 0);
    song.startGesture();

    EditContext gesture (Policy::updateAll);
    gesture.addPlacement (edited);

    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), gesture, song.undoManager).wasApplied());

    CHECK (song.clipCount() == 1);
    CHECK (song.clip (clipId).getNotes().size() == 1);
}

TEST_CASE ("The event clip commands refuse a step clip, a clip that is not there and a context of another clip", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto stepClipId = song.stepClip();
    const auto ofTheStepClip = song.place (0, stepClipId, 0);
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (0, 240, 60), first, song.undoManager).wasApplied());
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    const auto allRefuse = [&song] (const juce::String& id, EditContext& context) {
        const auto results = {
            commands::addNote (song.project, id, note (0, 240, 61), context, song.undoManager),
            commands::setNote (song.project, id, 0, note (0, 240, 61), context, song.undoManager),
            commands::removeNote (song.project, id, 0, context, song.undoManager),
            commands::addEvent (song.project, id, event (0, Kind::cc, 1, 1), context, song.undoManager),
            commands::setEvent (song.project, id, 0, event (0, Kind::cc, 1, 1), context, song.undoManager),
            commands::removeEvent (song.project, id, 0, context, song.undoManager),
            commands::setClipLoopLength (song.project, id, 960, context, song.undoManager),
            commands::setClipQuantise (song.project, id, "1/4", context, song.undoManager),
        };

        for (const auto& result : results)
        {
            CHECK (! result.wasApplied());
            CHECK (result.refusal.isNotEmpty());
        }
    };

    EditContext poolLevel (Policy::updateAll);
    allRefuse (stepClipId, poolLevel);
    allRefuse (unknownId, poolLevel);
    allRefuse ("", poolLevel);

    EditContext throughAnother (Policy::makeUnique);
    throughAnother.addPlacement (ofTheStepClip);
    allRefuse (clipId, throughAnother);

    CHECK (song.isUntouchedSince (before));
}

//==============================================================================
// Loop length.

TEST_CASE ("setClipLoopLength brings the offset of every placement of the clip back into the loop, in one undo step", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto laneClipId = song.eventClip();
    const auto otherClipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addEvent (song.project, clipId, event (0, Kind::cc, 74, 1), first, song.undoManager).wasApplied());

    // Offsets come from splitting: 0 | 1000 | 2000 | 3000 on the track, and
    // 0 | 960 on a lane, whose clip is on lanes only. The other clip's
    // placement has one of 3000.
    const auto whole = song.place (0, clipId, 0);
    const auto second = commands::splitPlacement (song.project, whole, 1000, song.undoManager);
    const auto third = commands::splitPlacement (song.project, second.createdId, 2000, song.undoManager);
    const auto fourth = commands::splitPlacement (song.project, third.createdId, 3000, song.undoManager);
    const auto onLane = song.placeOnALane (1, laneClipId, 0);
    const auto onLaneSecond = commands::splitPlacement (song.project, onLane, 960, song.undoManager);
    const auto ofOther = song.place (2, otherClipId, 0);
    const auto ofOtherSecond = commands::splitPlacement (song.project, ofOther, 3000, song.undoManager);
    REQUIRE (fourth.wasApplied());
    REQUIRE (onLaneSecond.wasApplied());
    REQUIRE (ofOtherSecond.wasApplied());
    REQUIRE (song.placement (fourth.createdId).getOffset() == 3000);

    const auto before = song.startGesture();

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::setClipLoopLength (song.project, clipId, 1000, poolLevel, song.undoManager).wasApplied());

    CHECK (song.isValid());
    CHECK (song.clip (clipId).getLoopLength() == 1000);
    CHECK (song.placement (whole).getOffset() == 0);
    CHECK (song.placement (second.createdId).getOffset() == 0);
    CHECK (song.placement (third.createdId).getOffset() == 0);
    CHECK (song.placement (fourth.createdId).getOffset() == 0);
    CHECK (song.placement (onLaneSecond.createdId).getOffset() == 960);
    CHECK (song.placement (ofOtherSecond.createdId).getOffset() == 3000);

    // An offset of 0 is not stored.
    CHECK (! commands::NodeLookup::placementWithId (song.project, second.createdId).hasProperty (ids::offset));

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    CHECK (song.placement (fourth.createdId).getOffset() == 3000);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);

    // A lane's placement is brought back like a track's: 960 mod 700.
    song.undoManager.beginNewTransaction();
    EditContext again (Policy::updateAll);
    REQUIRE (commands::setClipLoopLength (song.project, laneClipId, 700, again, song.undoManager).wasApplied());
    CHECK (song.placement (onLaneSecond.createdId).getOffset() == 260);
    CHECK (song.isValid());
}

TEST_CASE ("setClipLoopLength keeps the content, also what now lies past the loop, and an offset that is inside a longer loop", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (3000, 240, 60), first, song.undoManager).wasApplied());
    const auto whole = song.place (0, clipId, 0);
    const auto second = commands::splitPlacement (song.project, whole, 3000, song.undoManager);
    REQUIRE (second.wasApplied());

    EditContext shorter (Policy::updateAll);
    REQUIRE (commands::setClipLoopLength (song.project, clipId, 3001, shorter, song.undoManager).wasApplied());
    CHECK (song.placement (second.createdId).getOffset() == 3000);

    EditContext longer (Policy::updateAll);
    REQUIRE (commands::setClipLoopLength (song.project, clipId, largestTick, longer, song.undoManager).wasApplied());
    CHECK (song.placement (second.createdId).getOffset() == 3000);

    EditContext shortest (Policy::updateAll);
    REQUIRE (commands::setClipLoopLength (song.project, clipId, 1, shortest, song.undoManager).wasApplied());
    CHECK (song.placement (second.createdId).getOffset() == 0);

    CHECK (song.isValid());
    REQUIRE (song.clip (clipId).getNotes().size() == 1);
    CHECK (song.clip (clipId).getNotes()[0].getTick() == 3000);
}

TEST_CASE ("Under make unique setClipLoopLength changes the offsets of the clone's placements only", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto whole = song.place (0, clipId, 0);
    const auto edited = commands::splitPlacement (song.project, whole, 3000, song.undoManager);
    const auto otherWhole = song.place (1, clipId, 0);
    const auto other = commands::splitPlacement (song.project, otherWhole, 3000, song.undoManager);
    REQUIRE (edited.wasApplied());
    REQUIRE (other.wasApplied());
    const auto before = song.startGesture();

    EditContext gesture (Policy::makeUnique);
    gesture.addPlacement (edited.createdId);
    REQUIRE (commands::setClipLoopLength (song.project, clipId, 960, gesture, song.undoManager).wasApplied());

    CHECK (song.isValid());
    REQUIRE (gesture.hasClone());
    CHECK (song.clip (clipId).getLoopLength() == 3840);
    CHECK (song.clip (gesture.getCloneId()).getLoopLength() == 960);
    CHECK (song.placement (edited.createdId).getOffset() == 3000 % 960);
    CHECK (song.placement (other.createdId).getOffset() == 3000);

    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
}

TEST_CASE ("setClipLoopLength refuses a loop of no tick, and the length the clip has changes nothing", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto placementId = song.place (0, clipId, 0);
    song.place (1, clipId, 0);
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    for (const auto length : { std::int64_t { 0 }, std::int64_t { -1 }, std::int64_t { -3840 }, std::numeric_limits<std::int64_t>::min() })
    {
        EditContext poolLevel (Policy::updateAll);
        const auto result = commands::setClipLoopLength (song.project, clipId, length, poolLevel, song.undoManager);

        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    EditContext throughOne (Policy::makeUnique);
    throughOne.addPlacement (placementId);
    CHECK (commands::setClipLoopLength (song.project, clipId, 3840, throughOne, song.undoManager).wasApplied());
    CHECK (! throughOne.hasClone());

    CHECK (song.isUntouchedSince (before));
}

//==============================================================================
// Quantise.

TEST_CASE ("setClipQuantise takes the seven settings of the schema and leaves the notes where they are", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    EditContext first (Policy::updateAll);
    REQUIRE (commands::addNote (song.project, clipId, note (7, 240, 60), first, song.undoManager).wasApplied());

    for (const auto* setting : { "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T", "" })
    {
        EditContext poolLevel (Policy::updateAll);

        INFO (setting);
        CHECK (commands::setClipQuantise (song.project, clipId, setting, poolLevel, song.undoManager).wasApplied());
        CHECK (song.clip (clipId).getQuantise() == setting);
        CHECK (song.isValid());
    }

    CHECK (song.clip (clipId).getNotes()[0].getTick() == 7);
}

TEST_CASE ("setClipQuantise is undone in one step", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto before = song.startGesture();

    EditContext poolLevel (Policy::updateAll);
    REQUIRE (commands::setClipQuantise (song.project, clipId, "1/8", poolLevel, song.undoManager).wasApplied());
    CHECK (song.clip (clipId).getQuantiseGrid() == 480);

    const auto after = song.xml();
    REQUIRE (song.undoManager.undo());
    CHECK (song.xml() == before);
    REQUIRE (song.undoManager.redo());
    CHECK (song.xml() == after);
}

TEST_CASE ("setClipQuantise refuses any other text, and the setting the clip has changes nothing", "[model][commands][EventClipCommands]")
{
    Song song;
    const auto clipId = song.eventClip();
    const auto placementId = song.place (0, clipId, 0);
    song.place (1, clipId, 0);
    const auto before = song.startGesture();
    song.undoManager.clearUndoHistory();

    for (const auto* setting : { "1/16", "1/8t", "1/8 ", " ", "off", "8", "1/1" })
    {
        EditContext poolLevel (Policy::updateAll);
        const auto result = commands::setClipQuantise (song.project, clipId, setting, poolLevel, song.undoManager);

        INFO (setting);
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
    }

    EditContext throughOne (Policy::makeUnique);
    throughOne.addPlacement (placementId);
    CHECK (commands::setClipQuantise (song.project, clipId, "", throughOne, song.undoManager).wasApplied());
    CHECK (! throughOne.hasClone());

    CHECK (song.isUntouchedSince (before));
}
