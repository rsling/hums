// model::commands for Performers and Players (docs/SCHEMA.md -> Routing).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/RoutingModel.h"
#include "model/TrackModel.h"
#include "model/commands/RoutingCommands.h"

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";

model::RoutingModel routingOf (const CommandFixture& fixture)
{
    return model::RoutingModel (fixture.routing());
}

} // namespace

TEST_CASE ("addPerformer makes an unbound Performer with a new id, and undo takes it away again", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    const auto result = commands::addPerformer (fixture.project, "Pads", fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto performers = routingOf (fixture).getPerformers();
    REQUIRE (performers.size() == 2);
    CHECK (performers[1].getId() == result.createdId);
    CHECK (performers[1].getName() == "Pads");
    CHECK (performers[1].getDeviceIdentifier().isEmpty());
    CHECK (performers[1].getDeviceName().isEmpty());
    CHECK (performers[1].getChannelFilter() == 0);

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("Two ports that are added get two ids", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    const auto first = commands::addPlayer (fixture.project, "Odyssey", fixture.undoManager);
    const auto second = commands::addPlayer (fixture.project, "Pro-One", fixture.undoManager);

    REQUIRE (first.wasApplied());
    REQUIRE (second.wasApplied());
    CHECK (first.createdId != second.createdId);
    CHECK (fixture.isValid());
    CHECK (routingOf (fixture).getPlayers().size() == 4);
}

// The fixture has the Players "Minimoog" and "Prophet" and the Performer
// "Keyboard".
TEST_CASE ("A port is not given the name another port of its kind has, nor an empty one", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    const auto results = {
        commands::addPlayer (fixture.project, "Minimoog", fixture.undoManager),
        commands::addPlayer (fixture.project, "Prophet", fixture.undoManager),
        commands::addPlayer (fixture.project, "", fixture.undoManager),
        commands::addPerformer (fixture.project, "Keyboard", fixture.undoManager),
        commands::addPerformer (fixture.project, "", fixture.undoManager),
        commands::setPlayerName (fixture.project, CommandFixture::minimoogId, "Prophet", fixture.undoManager),
        commands::setPlayerName (fixture.project, CommandFixture::minimoogId, "", fixture.undoManager),
        commands::setPerformerName (fixture.project, CommandFixture::keyboardId, "", fixture.undoManager),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
        CHECK (result.createdId.isEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A Performer and a Player may share a name, and names are compared as they are written", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    // An input and an output of one device.
    CHECK (commands::addPerformer (fixture.project, "Minimoog", fixture.undoManager).wasApplied());
    CHECK (commands::addPlayer (fixture.project, "Keyboard", fixture.undoManager).wasApplied());
    CHECK (commands::setPerformerName (fixture.project, CommandFixture::keyboardId, "Prophet", fixture.undoManager).wasApplied());

    // Another case, a space more: another name.
    CHECK (commands::addPlayer (fixture.project, "minimoog", fixture.undoManager).wasApplied());
    CHECK (commands::addPlayer (fixture.project, "Minimoog ", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
}

TEST_CASE ("A name is free again once its port is renamed or removed", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setPlayerName (fixture.project, CommandFixture::minimoogId, "Model D", fixture.undoManager).wasApplied());
    const auto added = commands::addPlayer (fixture.project, "Minimoog", fixture.undoManager);
    REQUIRE (added.wasApplied());

    REQUIRE (commands::removePlayer (fixture.project, added.createdId, fixture.undoManager).wasApplied());
    CHECK (commands::setPlayerName (fixture.project, CommandFixture::minimoogId, "Minimoog", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("addPlayer makes an unbound Player that sends no clock", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto result = commands::addPlayer (fixture.project, "Odyssey", fixture.undoManager);

    REQUIRE (result.wasApplied());
    CHECK (fixture.isValid());

    const auto players = routingOf (fixture).getPlayers();
    REQUIRE (players.size() == 3);
    CHECK (players[2].getId() == result.createdId);
    CHECK (players[2].getName() == "Odyssey");
    CHECK (! players[2].getSendClock());
}

TEST_CASE ("removePlayer unassigns every track that played through it, in one undo step", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removePlayer (fixture.project, CommandFixture::minimoogId, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (routingOf (fixture).getPlayers().size() == 1);
    CHECK (routingOf (fixture).getPlayers()[0].getName() == "Prophet");

    CHECK (model::TrackModel (fixture.track (0)).getPlayerId().isEmpty());
    CHECK (model::TrackModel (fixture.track (1)).getPlayerId() == CommandFixture::prophetId);
    CHECK (model::TrackModel (fixture.track (2)).getPlayerId().isEmpty());
    // What the track records from is another matter.
    CHECK (model::TrackModel (fixture.track (0)).getPerformerId() == CommandFixture::keyboardId);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
}

TEST_CASE ("removePerformer unassigns every track that recorded from it, in one undo step", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removePerformer (fixture.project, CommandFixture::keyboardId, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (routingOf (fixture).getPerformers().empty());
    CHECK (model::TrackModel (fixture.track (0)).getPerformerId().isEmpty());
    CHECK (model::TrackModel (fixture.track (0)).getPlayerId() == CommandFixture::minimoogId);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());
}

TEST_CASE ("The properties of a Performer are set through commands", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setPerformerName (fixture.project, CommandFixture::keyboardId, "Keystep", fixture.undoManager).wasApplied());
    REQUIRE (commands::setPerformerDevice (fixture.project, CommandFixture::keyboardId, "usb:1234", "Arturia Keystep", fixture.undoManager).wasApplied());
    REQUIRE (commands::setPerformerChannelFilter (fixture.project, CommandFixture::keyboardId, 16, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto performer = routingOf (fixture).getPerformers()[0];
    CHECK (performer.getName() == "Keystep");
    CHECK (performer.getDeviceIdentifier() == "usb:1234");
    CHECK (performer.getDeviceName() == "Arturia Keystep");
    CHECK (performer.getChannelFilter() == 16);

    REQUIRE (commands::setPerformerChannelFilter (fixture.project, CommandFixture::keyboardId, 0, fixture.undoManager).wasApplied());
    CHECK (routingOf (fixture).getPerformers()[0].getChannelFilter() == 0);
}

TEST_CASE ("The properties of a Player are set through commands", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setPlayerName (fixture.project, CommandFixture::prophetId, "Prophet-5", fixture.undoManager).wasApplied());
    REQUIRE (commands::setPlayerDevice (fixture.project, CommandFixture::prophetId, "usb:42", "MOTU midi express 2", fixture.undoManager).wasApplied());
    REQUIRE (commands::setPlayerSendClock (fixture.project, CommandFixture::prophetId, true, fixture.undoManager).wasApplied());
    REQUIRE (commands::setPlayerSendClock (fixture.project, CommandFixture::minimoogId, false, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto players = routingOf (fixture).getPlayers();
    CHECK (! players[0].getSendClock());
    CHECK (players[1].getName() == "Prophet-5");
    CHECK (players[1].getDeviceIdentifier() == "usb:42");
    CHECK (players[1].getDeviceName() == "MOTU midi express 2");
    CHECK (players[1].getSendClock());
}

TEST_CASE ("A device is set as one undo step, and unbinding empties both of its texts", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setPlayerDevice (fixture.project, CommandFixture::prophetId, "usb:42", "MOTU", fixture.undoManager).wasApplied());
    const auto bound = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setPlayerDevice (fixture.project, CommandFixture::prophetId, "", "", fixture.undoManager).wasApplied());

    CHECK (routingOf (fixture).getPlayers()[1].getDeviceIdentifier().isEmpty());
    CHECK (routingOf (fixture).getPlayers()[1].getDeviceName().isEmpty());

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == bound);
}

TEST_CASE ("Setting a property to the value it has is applied and leaves nothing to undo", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (commands::setPlayerName (fixture.project, CommandFixture::minimoogId, "Minimoog", fixture.undoManager).wasApplied());
    // Held as the text "1" in the fixture, set as a number here.
    CHECK (commands::setPlayerSendClock (fixture.project, CommandFixture::minimoogId, true, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A routing command for a port that is not there is refused and changes nothing", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    const auto results = {
        commands::removePerformer (fixture.project, unknownId, fixture.undoManager),
        commands::setPerformerName (fixture.project, unknownId, "x", fixture.undoManager),
        commands::setPerformerDevice (fixture.project, unknownId, "x", "y", fixture.undoManager),
        commands::setPerformerChannelFilter (fixture.project, unknownId, 1, fixture.undoManager),
        commands::removePlayer (fixture.project, unknownId, fixture.undoManager),
        commands::setPlayerName (fixture.project, unknownId, "x", fixture.undoManager),
        commands::setPlayerDevice (fixture.project, unknownId, "x", "y", fixture.undoManager),
        commands::setPlayerSendClock (fixture.project, unknownId, true, fixture.undoManager),
        // A Player's id names no Performer, and the other way round.
        commands::removePerformer (fixture.project, CommandFixture::minimoogId, fixture.undoManager),
        commands::setPlayerName (fixture.project, CommandFixture::keyboardId, "x", fixture.undoManager),
        commands::removePlayer (fixture.project, "", fixture.undoManager),
    };

    for (const auto& result : results)
    {
        CHECK (! result.wasApplied());
        CHECK (result.refusal.isNotEmpty());
        CHECK (result.createdId.isEmpty());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A channel filter outside 0 to 16 is refused", "[model][commands][RoutingCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setPerformerChannelFilter (fixture.project, CommandFixture::keyboardId, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::setPerformerChannelFilter (fixture.project, CommandFixture::keyboardId, 17, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}
