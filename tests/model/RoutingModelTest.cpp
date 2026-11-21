// model::RoutingModel: logical ports and the pad map. docs/SCHEMA.md -> Routing.

#include <catch2/catch_test_macros.hpp>

#include "model/RoutingModel.h"

namespace
{

juce::ValueTree berlinRouting()
{
    return juce::ValueTree::fromXml (
        R"(<Routing>
             <Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>
             <Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>
             <Player id="0a1b2c3d4e5f40617283940a1b2c3d4e" name="Prophet"/>
             <Performer id="c4d5e6f708194a2b8c3d4e5f60718293" name="Pads"/>
             <PadMap>
               <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
             </PadMap>
           </Routing>)");
}

} // namespace

TEST_CASE ("Routing lists Performers and Players separately, each in document order", "[model][RoutingModel]")
{
    const model::RoutingModel routing (berlinRouting());

    const auto performers = routing.getPerformers();
    REQUIRE (performers.size() == 2);
    CHECK (performers[0].getName() == "Keyboard");
    CHECK (performers[1].getName() == "Pads");

    const auto players = routing.getPlayers();
    REQUIRE (players.size() == 2);
    CHECK (players[0].getName() == "Minimoog");
    CHECK (players[0].getSendClock() == true);
    CHECK (players[1].getName() == "Prophet");
    CHECK (players[1].getSendClock() == false);
}

TEST_CASE ("Routing gives its PadMap", "[model][RoutingModel]")
{
    const model::RoutingModel routing (berlinRouting());

    const auto pads = routing.getPadMap().getPads();

    REQUIRE (pads.size() == 1);
    CHECK (pads[0].getLabel() == "Kick");
}

TEST_CASE ("Routing without ports has none", "[model][RoutingModel]")
{
    const model::RoutingModel routing (juce::ValueTree::fromXml ("<Routing><PadMap/></Routing>"));

    CHECK (routing.getPerformers().empty());
    CHECK (routing.getPlayers().empty());
    CHECK (routing.getPadMap().getPads().empty());
}
