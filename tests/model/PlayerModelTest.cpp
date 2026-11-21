// model::PlayerModel: a logical MIDI output. docs/SCHEMA.md -> Routing.

#include <catch2/catch_test_macros.hpp>

#include "model/PlayerModel.h"

TEST_CASE ("A Player gives back what is stored", "[model][PlayerModel]")
{
    const model::PlayerModel player (juce::ValueTree::fromXml (
        R"(<Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog"
                   deviceIdentifier="1873406112" deviceName="MOTU midi express 1" sendClock="1"/>)"));

    CHECK (player.getId() == "9b1d6f3a2c4e4f708192a3b4c5d6e7f8");
    CHECK (player.getName() == "Minimoog");
    CHECK (player.getDeviceIdentifier() == "1873406112");
    CHECK (player.getDeviceName() == "MOTU midi express 1");
    CHECK (player.getSendClock() == true);
}

TEST_CASE ("A Player that was never bound has no device and sends no clock", "[model][PlayerModel]")
{
    const model::PlayerModel player (juce::ValueTree::fromXml (
        R"(<Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog"/>)"));

    CHECK (player.getDeviceIdentifier() == "");
    CHECK (player.getDeviceName() == "");
    CHECK (player.getSendClock() == false);
}
