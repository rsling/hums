// model::PerformerModel: a logical MIDI input. docs/SCHEMA.md -> Routing.

#include <catch2/catch_test_macros.hpp>

#include "model/PerformerModel.h"

TEST_CASE ("A Performer gives back what is stored", "[model][PerformerModel]")
{
    const model::PerformerModel performer (juce::ValueTree::fromXml (
        R"(<Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"
                      deviceIdentifier="-1529834617" deviceName="Keystep" channelFilter="3"/>)"));

    CHECK (performer.getId() == "3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1");
    CHECK (performer.getName() == "Keyboard");
    CHECK (performer.getDeviceIdentifier() == "-1529834617");
    CHECK (performer.getDeviceName() == "Keystep");
    CHECK (performer.getChannelFilter() == 3);
}

TEST_CASE ("A Performer that was never bound has no device and listens on every channel", "[model][PerformerModel]")
{
    const model::PerformerModel performer (juce::ValueTree::fromXml (
        R"(<Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>)"));

    CHECK (performer.getDeviceIdentifier() == "");
    CHECK (performer.getDeviceName() == "");
    CHECK (performer.getChannelFilter() == 0);
}
