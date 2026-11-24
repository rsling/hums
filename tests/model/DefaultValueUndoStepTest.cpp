// A command that sets a property to its default, on a node that does not
// store the property, changes nothing the wrappers or a saved file can show.
// It therefore writes nothing: the default stays unstored and nothing is left
// to undo, as with the clip commands (ClipPoolCommands.cpp, setClipText).
// Before, each of these commands stored the default and left an undo step
// (finding 14 of the milestone 1 code review, 23 Nov 2026).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/PadMapCommands.h"
#include "model/commands/RoutingCommands.h"
#include "model/commands/SessionCommands.h"
#include "model/commands/TimelineCommands.h"
#include "model/commands/TrackCommands.h"

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const bassId = "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b";
const char* const leadId = "5d7f9b1d3f504c2e9b7d5f3b1d2f4a6c";

// Takes a property away without a trace in the undo history, so that the
// node reads as its default, and leaves the history empty.
void makeUnstored (CommandFixture& fixture, juce::ValueTree node, const juce::Identifier& property)
{
    node.removeProperty (property, nullptr);
    fixture.undoManager.clearUndoHistory();

    REQUIRE (fixture.isValid());
    REQUIRE_FALSE (node.hasProperty (property));
}

// The command was applied, the default is still unstored, and there is
// nothing to undo.
void checkNothingWasLeft (const CommandFixture& fixture, const commands::CommandResult& result, const juce::ValueTree& node, const juce::Identifier& property)
{
    REQUIRE (result.wasApplied());

    CHECK_FALSE (node.hasProperty (property));
    CHECK (fixture.undoManager.getNumActionsInCurrentTransaction() == 0);
    CHECK_FALSE (fixture.undoManager.canUndo());
}

} // namespace

TEST_CASE ("control: a value that is stored already leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;
    auto track = fixture.track (0);
    track.setProperty (ids::mute, 0, nullptr);

    const auto result = commands::setTrackMute (fixture.project, bassId, false, fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.undoManager.getNumActionsInCurrentTransaction() == 0);
    CHECK_FALSE (fixture.undoManager.canUndo());
}

TEST_CASE ("a Track command that sets an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;
    auto track = fixture.track (0);

    SECTION ("setTrackName")
    {
        makeUnstored (fixture, track, ids::name);
        checkNothingWasLeft (fixture, commands::setTrackName (fixture.project, bassId, "", fixture.undoManager), track, ids::name);
    }

    SECTION ("setTrackColour")
    {
        makeUnstored (fixture, track, ids::colour);
        checkNothingWasLeft (fixture, commands::setTrackColour (fixture.project, bassId, "", fixture.undoManager), track, ids::colour);
    }

    SECTION ("setTrackMute")
    {
        makeUnstored (fixture, track, ids::mute);
        checkNothingWasLeft (fixture, commands::setTrackMute (fixture.project, bassId, false, fixture.undoManager), track, ids::mute);
    }

    SECTION ("setTrackSolo")
    {
        makeUnstored (fixture, track, ids::solo);
        checkNothingWasLeft (fixture, commands::setTrackSolo (fixture.project, bassId, false, fixture.undoManager), track, ids::solo);
    }

    SECTION ("setTrackPlayer")
    {
        makeUnstored (fixture, track, ids::playerId);
        checkNothingWasLeft (fixture, commands::setTrackPlayer (fixture.project, bassId, "", fixture.undoManager), track, ids::playerId);
    }

    SECTION ("setTrackPerformer")
    {
        auto lead = fixture.track (1);
        makeUnstored (fixture, lead, ids::performerId);
        checkNothingWasLeft (fixture, commands::setTrackPerformer (fixture.project, leadId, "", fixture.undoManager), lead, ids::performerId);
    }

    SECTION ("setTrackChannel")
    {
        makeUnstored (fixture, track, ids::channel);
        checkNothingWasLeft (fixture, commands::setTrackChannel (fixture.project, bassId, 1, fixture.undoManager), track, ids::channel);
    }

    SECTION ("setKnobLabel")
    {
        REQUIRE (commands::addKnob (fixture.project, bassId, 74, "", fixture.undoManager).wasApplied());
        auto knob = track.getChildWithName (ids::Knobs).getChild (0);
        REQUIRE (knob.isValid());

        makeUnstored (fixture, knob, ids::label);
        checkNothingWasLeft (fixture, commands::setKnobLabel (fixture.project, bassId, 74, "", fixture.undoManager), knob, ids::label);
    }
}

TEST_CASE ("a Routing command that sets an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;
    auto performer = fixture.routing().getChildWithName (ids::Performer);
    auto prophet = fixture.routing().getChildWithProperty (ids::id, CommandFixture::prophetId);

    SECTION ("setPerformerDevice")
    {
        makeUnstored (fixture, performer, ids::deviceIdentifier);
        makeUnstored (fixture, performer, ids::deviceName);

        const auto result = commands::setPerformerDevice (fixture.project, CommandFixture::keyboardId, "", "", fixture.undoManager);

        CHECK_FALSE (performer.hasProperty (ids::deviceName));
        checkNothingWasLeft (fixture, result, performer, ids::deviceIdentifier);
    }

    SECTION ("setPlayerDevice")
    {
        makeUnstored (fixture, prophet, ids::deviceIdentifier);
        makeUnstored (fixture, prophet, ids::deviceName);

        const auto result = commands::setPlayerDevice (fixture.project, CommandFixture::prophetId, "", "", fixture.undoManager);

        CHECK_FALSE (prophet.hasProperty (ids::deviceName));
        checkNothingWasLeft (fixture, result, prophet, ids::deviceIdentifier);
    }

    SECTION ("setPerformerChannelFilter")
    {
        makeUnstored (fixture, performer, ids::channelFilter);
        checkNothingWasLeft (fixture, commands::setPerformerChannelFilter (fixture.project, CommandFixture::keyboardId, 0, fixture.undoManager), performer, ids::channelFilter);
    }

    SECTION ("setPlayerSendClock")
    {
        makeUnstored (fixture, prophet, ids::sendClock);
        checkNothingWasLeft (fixture, commands::setPlayerSendClock (fixture.project, CommandFixture::prophetId, false, fixture.undoManager), prophet, ids::sendClock);
    }
}

TEST_CASE ("setPadLabel with an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;
    auto pad = fixture.routing().getChildWithName (ids::PadMap).getChild (1);

    makeUnstored (fixture, pad, ids::label);
    checkNothingWasLeft (fixture, commands::setPadLabel (fixture.project, 1, "", fixture.undoManager), pad, ids::label);
}

TEST_CASE ("renameMarker with an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;

    // The last child of the fixture's Timeline is the marker without a name.
    auto marker = fixture.timeline().getChild (fixture.timeline().getNumChildren() - 1);
    REQUIRE (marker.hasType (ids::Marker));

    makeUnstored (fixture, marker, ids::name);
    checkNothingWasLeft (fixture, commands::renameMarker (fixture.project, marker, "", fixture.undoManager), marker, ids::name);
}

TEST_CASE ("a Session command that sets an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;

    SECTION ("setLaunchQuantise")
    {
        auto session = fixture.session();
        makeUnstored (fixture, session, ids::launchQuantise);
        checkNothingWasLeft (fixture, commands::setLaunchQuantise (fixture.project, 3840, fixture.undoManager), session, ids::launchQuantise);
    }

    SECTION ("setSceneName")
    {
        auto scene = fixture.session().getChildWithName (ids::Scene);
        makeUnstored (fixture, scene, ids::name);
        checkNothingWasLeft (fixture, commands::setSceneName (fixture.project, 2, "", fixture.undoManager), scene, ids::name);
    }
}

TEST_CASE ("setAutomationLaneName with an unstored default leaves no undo step", "[model][commands]")
{
    CommandFixture fixture;

    const auto added = commands::addAutomationLane (fixture.project, bassId, "", fixture.undoManager);
    REQUIRE (added.wasApplied());

    auto lane = fixture.track (0).getChildWithName (ids::AutomationLanes).getChild (0);
    REQUIRE (lane.isValid());

    makeUnstored (fixture, lane, ids::name);
    checkNothingWasLeft (fixture, commands::setAutomationLaneName (fixture.project, added.createdId, "", fixture.undoManager), lane, ids::name);
}
