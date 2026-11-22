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
