// model::AutomationClipRule: what an AutomationLane requires of its clips
// (docs/SCHEMA.md -> Placements / Placement, invariant 9).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/AutomationClipRule.h"
#include "model/ClipModel.h"
#include "model/TrackModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/SlotCommands.h"

#include <cstdint>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;
using model::AutomationClipRule;

// The node of a new event clip of one bar.
juce::ValueTree addedEventClip (CommandFixture& fixture)
{
    const auto result = commands::addEventClip (fixture.project, "Sweep", 3840, fixture.undoManager);
    REQUIRE (result.wasApplied());

    return fixture.project.getChildWithName (ids::ClipPool).getChildWithProperty (ids::id, result.createdId);
}

// Content is put into the tree as a loaded file would hold it: the commands
// for it are not what is tested here.
void addEvent (juce::ValueTree clip, const juce::String& kind, std::int64_t tick, bool enabled = true)
{
    juce::ValueTree event (ids::Event);
    event.setProperty (ids::tick, static_cast<juce::int64> (tick), nullptr);
    event.setProperty (ids::kind, kind, nullptr);

    if (! enabled)
        event.setProperty (ids::enabled, 0, nullptr);

    clip.getChildWithName (ids::Events).appendChild (event, nullptr);
}

void addNote (juce::ValueTree clip, std::int64_t tick, bool enabled = true)
{
    juce::ValueTree note (ids::Note);
    note.setProperty (ids::tick, static_cast<juce::int64> (tick), nullptr);
    note.setProperty (ids::length, 240, nullptr);
    note.setProperty (ids::pitch, 60, nullptr);

    if (! enabled)
        note.setProperty (ids::enabled, 0, nullptr);

    clip.getChildWithName (ids::Events).appendChild (note, nullptr);
}

} // namespace

TEST_CASE ("A lane can reference an event clip that is empty or holds only cc events", "[model][AutomationClipRule]")
{
    CommandFixture fixture;
    auto clip = addedEventClip (fixture);

    CHECK (AutomationClipRule::whyNotForALane (clip).isEmpty());

    addEvent (clip, "cc", 0);
    addEvent (clip, "cc", 5000); // past the loop
    addEvent (clip, "cc", 960, false);
    REQUIRE (fixture.isValid());

    CHECK (AutomationClipRule::whyNotForALane (clip).isEmpty());
}

TEST_CASE ("A lane cannot reference a step clip", "[model][AutomationClipRule]")
{
    CommandFixture fixture;
    const auto result = commands::addStepClip (fixture.project, "Bass", fixture.undoManager);
    REQUIRE (result.wasApplied());

    const auto clip = fixture.project.getChildWithName (ids::ClipPool).getChildWithProperty (ids::id, result.createdId);

    CHECK (AutomationClipRule::whyNotForALane (clip).contains ("step clip"));
}

TEST_CASE ("A lane cannot reference a clip with a note, also a disabled one and one past the loop", "[model][AutomationClipRule]")
{
    CommandFixture fixture;

    auto withNote = addedEventClip (fixture);
    addEvent (withNote, "cc", 0);
    addNote (withNote, 0);

    auto withDisabledNote = addedEventClip (fixture);
    addNote (withDisabledNote, 0, false);

    auto withNotePastTheLoop = addedEventClip (fixture);
    addNote (withNotePastTheLoop, 9000);

    REQUIRE (fixture.isValid());

    CHECK (AutomationClipRule::whyNotForALane (withNote).contains ("notes"));
    CHECK (AutomationClipRule::whyNotForALane (withDisabledNote).contains ("notes"));
    CHECK (AutomationClipRule::whyNotForALane (withNotePastTheLoop).contains ("notes"));
}

TEST_CASE ("A lane cannot reference a clip with an event of another kind than cc", "[model][AutomationClipRule]")
{
    for (const auto* kind : { "pitchBend", "programChange", "channelPressure", "polyPressure", "sysex" })
    {
        CommandFixture fixture;
        auto clip = addedEventClip (fixture);
        addEvent (clip, "cc", 0);
        addEvent (clip, kind, 0, false);
        REQUIRE (fixture.isValid());

        INFO (kind);
        CHECK (AutomationClipRule::whyNotForALane (clip).contains ("other kinds"));
    }
}

TEST_CASE ("isOnALane is true for a clip a lane placement references, and for no other reference", "[model][AutomationClipRule]")
{
    CommandFixture fixture;
    const auto trackId = model::TrackModel (fixture.track (0)).getId();
    const auto onLane = model::ClipModel (addedEventClip (fixture)).getId();
    const auto elsewhere = model::ClipModel (addedEventClip (fixture)).getId();
    const auto nowhere = model::ClipModel (addedEventClip (fixture)).getId();

    const auto lane = commands::addAutomationLane (fixture.project, trackId, "Cutoff", fixture.undoManager);
    REQUIRE (lane.wasApplied());
    const auto placement = commands::addLanePlacement (fixture.project, lane.createdId, onLane, 0, 3840, fixture.undoManager);
    REQUIRE (placement.wasApplied());

    // A track's own placement and a slot are no lane.
    REQUIRE (commands::addPlacement (fixture.project, trackId, elsewhere, 0, 3840, fixture.undoManager).wasApplied());
    REQUIRE (commands::setSlot (fixture.project, trackId, 0, elsewhere, fixture.undoManager).wasApplied());
    REQUIRE (fixture.isValid());

    CHECK (AutomationClipRule::isOnALane (fixture.project, onLane));
    CHECK (! AutomationClipRule::isOnALane (fixture.project, elsewhere));
    CHECK (! AutomationClipRule::isOnALane (fixture.project, nowhere));
    CHECK (! AutomationClipRule::isOnALane (fixture.project, "ffffffffffffffffffffffffffffffff"));

    REQUIRE (commands::removePlacement (fixture.project, placement.createdId, fixture.undoManager).wasApplied());
    CHECK (! AutomationClipRule::isOnALane (fixture.project, onLane));
}

TEST_CASE ("isOnATrackOrInASlot is true for a clip a track's own placement or a slot references, and not for a lane's clip", "[model][AutomationClipRule]")
{
    CommandFixture fixture;
    const auto trackId = model::TrackModel (fixture.track (0)).getId();
    const auto onLane = model::ClipModel (addedEventClip (fixture)).getId();
    const auto onTrack = model::ClipModel (addedEventClip (fixture)).getId();
    const auto inSlot = model::ClipModel (addedEventClip (fixture)).getId();
    const auto nowhere = model::ClipModel (addedEventClip (fixture)).getId();

    const auto lane = commands::addAutomationLane (fixture.project, trackId, "Cutoff", fixture.undoManager);
    REQUIRE (lane.wasApplied());
    REQUIRE (commands::addLanePlacement (fixture.project, lane.createdId, onLane, 0, 3840, fixture.undoManager).wasApplied());
    const auto placement = commands::addPlacement (fixture.project, trackId, onTrack, 0, 3840, fixture.undoManager);
    REQUIRE (placement.wasApplied());
    REQUIRE (commands::setSlot (fixture.project, trackId, 0, inSlot, fixture.undoManager).wasApplied());
    REQUIRE (fixture.isValid());

    CHECK (AutomationClipRule::isOnATrackOrInASlot (fixture.project, onTrack));
    CHECK (AutomationClipRule::isOnATrackOrInASlot (fixture.project, inSlot));
    CHECK (! AutomationClipRule::isOnATrackOrInASlot (fixture.project, onLane));
    CHECK (! AutomationClipRule::isOnATrackOrInASlot (fixture.project, nowhere));
    CHECK (! AutomationClipRule::isOnATrackOrInASlot (fixture.project, "ffffffffffffffffffffffffffffffff"));

    REQUIRE (commands::removePlacement (fixture.project, placement.createdId, fixture.undoManager).wasApplied());
    REQUIRE (commands::clearSlot (fixture.project, trackId, 0, fixture.undoManager).wasApplied());
    CHECK (! AutomationClipRule::isOnATrackOrInASlot (fixture.project, onTrack));
    CHECK (! AutomationClipRule::isOnATrackOrInASlot (fixture.project, inSlot));
}
