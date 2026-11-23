// An edit context that has cloned and is kept past its gesture is known as
// stale whatever the later gesture has done (docs/EDITING.md: "That is
// certain once the context has cloned"). Finding 7 of the milestone 1 code
// review (23 Nov 2026): it was told by a count of the transaction's actions,
// which a later gesture reaches, and its command then changed the clone of
// the earlier gesture.
//
// The command itself is not called with the stale context: the gate asserts
// on one, and a failed assertion fails the test (RefcountGateTest.cpp). What
// the gate asks is what is held here.

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/EditContext.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/RefcountGate.h"

#include <cstdint>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;
using commands::EditContext;
using Policy = model::Settings::SharedEditPolicy;

juce::String trackIdAt (const CommandFixture& fixture, int trackPosition)
{
    return model::TrackModel (fixture.track (trackPosition)).getId();
}

juce::String placeRiff (CommandFixture& fixture, int trackPosition, const juce::String& clipId)
{
    const auto result = commands::addPlacement (fixture.project, trackIdAt (fixture, trackPosition), clipId, 0, 960, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

juce::String nameOfClip (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::ClipModel (commands::NodeLookup::childWithId (fixture.project.getChildWithName (ids::ClipPool), ids::Clip, clipId)).getName();
}

int refcountOfClip (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getRefcount (clipId);
}

// A clip "Riff" on two tracks, and a first gesture that has renamed it
// through the placement on Lead under make unique, so its context holds a
// clone named "Riff B".
struct GestureThatHasCloned
{
    CommandFixture fixture;
    juce::String clipId;
    juce::String onBass;
    juce::String onLead;
    EditContext context { Policy::makeUnique };
    int actionsOfTheGesture = 0;

    GestureThatHasCloned()
    {
        const auto added = commands::addEventClip (fixture.project, "Riff", 3840, fixture.undoManager);
        REQUIRE (added.wasApplied());
        clipId = added.createdId;
        onBass = placeRiff (fixture, 0, clipId);
        onLead = placeRiff (fixture, 1, clipId);
        fixture.undoManager.clearUndoHistory();

        fixture.undoManager.beginNewTransaction();
        context.addPlacement (onLead);
        REQUIRE (commands::setClipName (fixture.project, clipId, "Riff B", context, fixture.undoManager).wasApplied());
        REQUIRE (context.hasClone());
        REQUIRE (nameOfClip (fixture, context.getCloneId()) == "Riff B");

        actionsOfTheGesture = fixture.undoManager.getNumActionsInCurrentTransaction();
        REQUIRE (actionsOfTheGesture > 0);
    }

    // A later gesture that does `count` things that have nothing to do with
    // the clone: it adds clips to the pool.
    void laterGestureAddsClips (int count)
    {
        for (int i = 0; i < count; ++i)
            REQUIRE (commands::addEventClip (fixture.project, "Other " + juce::String (i), 3840, fixture.undoManager).wasApplied());
    }
};

} // namespace

TEST_CASE ("A context that has cloned is stale whatever number of things the later gesture has done", "[model][commands][EditContext]")
{
    GestureThatHasCloned earlier;
    auto& fixture = earlier.fixture;

    // The later gesture does one thing after another; the context of the
    // earlier one must be known as stale at every count.
    fixture.undoManager.beginNewTransaction();
    CHECK (earlier.context.belongsToAnEarlierGesture (fixture.undoManager));

    int firstCountNotKnownAsStale = -1;

    for (int count = 1; count <= earlier.actionsOfTheGesture + 2; ++count)
    {
        earlier.laterGestureAddsClips (1);
        REQUIRE (fixture.undoManager.getNumActionsInCurrentTransaction() == count);

        if (firstCountNotKnownAsStale < 0 && ! earlier.context.belongsToAnEarlierGesture (fixture.undoManager))
            firstCountNotKnownAsStale = count;
    }

    CAPTURE (earlier.actionsOfTheGesture, firstCountNotKnownAsStale);
    CHECK (firstCountNotKnownAsStale == -1);
}

TEST_CASE ("A context that has cloned is stale also when the later gesture has done as much as its own", "[model][commands][EditContext]")
{
    GestureThatHasCloned earlier;
    auto& fixture = earlier.fixture;

    fixture.undoManager.beginNewTransaction();
    earlier.laterGestureAddsClips (earlier.actionsOfTheGesture);
    REQUIRE (fixture.undoManager.getNumActionsInCurrentTransaction() == earlier.actionsOfTheGesture);

    CHECK (earlier.context.belongsToAnEarlierGesture (fixture.undoManager));
}

TEST_CASE ("A context that has cloned is stale also when the later gesture has removed the reference it edited through", "[model][commands][EditContext]")
{
    GestureThatHasCloned earlier;
    auto& fixture = earlier.fixture;
    const auto cloneId = earlier.context.getCloneId();

    // The later gesture removes the placement the earlier one edited
    // through, and does enough besides.
    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removePlacement (fixture.project, earlier.onLead, fixture.undoManager).wasApplied());
    earlier.laterGestureAddsClips (earlier.actionsOfTheGesture);
    REQUIRE (refcountOfClip (fixture, cloneId) == 0);

    CHECK (earlier.context.belongsToAnEarlierGesture (fixture.undoManager));
}

TEST_CASE ("A context that has cloned stays the context of its gesture however much the gesture goes on to do", "[model][commands][EditContext]")
{
    GestureThatHasCloned gesture;
    auto& fixture = gesture.fixture;

    // More of the same gesture, with and without the context.
    gesture.laterGestureAddsClips (gesture.actionsOfTheGesture + 2);
    CHECK (! gesture.context.belongsToAnEarlierGesture (fixture.undoManager));

    REQUIRE (commands::setClipName (fixture.project, gesture.clipId, "Riff C", gesture.context, fixture.undoManager).wasApplied());
    CHECK (nameOfClip (fixture, gesture.context.getCloneId()) == "Riff C");
    CHECK (nameOfClip (fixture, gesture.clipId) == "Riff");
    CHECK (! gesture.context.belongsToAnEarlierGesture (fixture.undoManager));
}

TEST_CASE ("A context that has cloned is stale once its gesture was undone and redone, though the clone is there again", "[model][commands][EditContext]")
{
    GestureThatHasCloned earlier;
    auto& fixture = earlier.fixture;

    REQUIRE (fixture.undoManager.undo());
    REQUIRE (fixture.undoManager.redo());
    REQUIRE (nameOfClip (fixture, earlier.context.getCloneId()) == "Riff B");
    CHECK (earlier.context.belongsToAnEarlierGesture (fixture.undoManager));

    // Also when the next gesture has done as much.
    earlier.laterGestureAddsClips (earlier.actionsOfTheGesture);
    CHECK (earlier.context.belongsToAnEarlierGesture (fixture.undoManager));
}
