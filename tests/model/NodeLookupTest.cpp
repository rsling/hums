// model::commands::NodeLookup: how the commands find the node they change.

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/commands/NodeLookup.h"

namespace
{

namespace ids = model::ids;
using model::commands::NodeLookup;

} // namespace

TEST_CASE ("NodeLookup lists the children of one type in document order", "[model][NodeLookup]")
{
    const CommandFixture fixture;
    const auto players = NodeLookup::childrenOfType (fixture.routing(), ids::Player);

    REQUIRE (players.size() == 2);
    CHECK (players[0][ids::name].toString() == "Minimoog");
    CHECK (players[1][ids::name].toString() == "Prophet");
    CHECK (NodeLookup::childrenOfType (fixture.routing(), ids::Track).empty());
}

TEST_CASE ("NodeLookup finds a child by its id only among the children of the type asked for", "[model][NodeLookup]")
{
    const CommandFixture fixture;

    CHECK (NodeLookup::childWithId (fixture.routing(), ids::Player, CommandFixture::prophetId)[ids::name].toString() == "Prophet");
    CHECK (! NodeLookup::childWithId (fixture.routing(), ids::Performer, CommandFixture::prophetId).isValid());
    CHECK (! NodeLookup::childWithId (fixture.routing(), ids::Player, "").isValid());
}

TEST_CASE ("NodeLookup finds a child by a number, whether it is held as text or as a number", "[model][NodeLookup]")
{
    CommandFixture fixture;
    auto timeline = fixture.timeline();

    juce::ValueTree added (ids::TempoEvent);
    added.setProperty (ids::tick, juce::int64 { 1 } << 40, nullptr);
    added.setProperty (ids::bpm, "90", nullptr);
    timeline.appendChild (added, nullptr);

    CHECK (NodeLookup::childWithNumber (timeline, ids::TempoEvent, ids::tick, 7680)[ids::bpm].toString() == "124.5");
    CHECK (NodeLookup::childWithNumber (timeline, ids::TempoEvent, ids::tick, std::int64_t { 1 } << 40) == added);
    CHECK (! NodeLookup::childWithNumber (timeline, ids::TempoEvent, ids::tick, 3840).isValid());

    // A child without the property takes no part, also when 0 is asked for.
    CHECK (! NodeLookup::childWithNumber (fixture.routing(), ids::Player, ids::channelFilter, 0).isValid());
}

TEST_CASE ("NodeLookup finds a placement by its id among a track's own and on its automation lanes", "[model][NodeLookup]")
{
    CommandFixture fixture;

    const auto placement = [] (const char* id) {
        juce::ValueTree node (ids::Placement);
        node.setProperty (ids::id, id, nullptr);
        return node;
    };

    juce::ValueTree lane (ids::AutomationLane);
    lane.setProperty (ids::id, "a lane", nullptr);
    lane.appendChild (placement ("on the lane"), nullptr);

    fixture.track (0).getChildWithName (ids::Placements).appendChild (placement ("on the first track"), nullptr);
    fixture.track (2).getChildWithName (ids::Placements).appendChild (placement ("on the third track"), nullptr);
    fixture.track (1).getChildWithName (ids::AutomationLanes).appendChild (lane, nullptr);

    CHECK (NodeLookup::placementWithId (fixture.project, "on the first track").getParent().getParent() == fixture.track (0));
    CHECK (NodeLookup::placementWithId (fixture.project, "on the third track").getParent().getParent() == fixture.track (2));
    CHECK (NodeLookup::placementWithId (fixture.project, "on the lane").getParent() == lane);

    // A lane's id names no placement, nor does a track's.
    CHECK (! NodeLookup::placementWithId (fixture.project, "a lane").isValid());
    CHECK (! NodeLookup::placementWithId (fixture.project, "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b").isValid());
    CHECK (! NodeLookup::placementWithId (fixture.project, "").isValid());
}
