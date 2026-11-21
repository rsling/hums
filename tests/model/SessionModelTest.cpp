// model::SessionModel: launch quantise and scene names. docs/SCHEMA.md -> Session.

#include <catch2/catch_test_macros.hpp>

#include "model/SessionModel.h"

TEST_CASE ("A Session gives back its launch quantise", "[model][SessionModel]")
{
    const model::SessionModel session (juce::ValueTree::fromXml (R"(<Session launchQuantise="960"/>)"));

    CHECK (session.getLaunchQuantise() == 960);
}

TEST_CASE ("A Session can launch immediately", "[model][SessionModel]")
{
    const model::SessionModel session (juce::ValueTree::fromXml (R"(<Session launchQuantise="0"/>)"));

    CHECK (session.getLaunchQuantise() == 0);
}

TEST_CASE ("A Session without a launch quantise launches on a one-bar grid", "[model][SessionModel]")
{
    const model::SessionModel session (juce::ValueTree::fromXml ("<Session/>"));

    // 3840 ticks: four quarter notes at 960.
    CHECK (session.getLaunchQuantise() == 3840);
    CHECK (session.getScenes().empty());
}

TEST_CASE ("A Session lists only the scenes that have a node, in document order", "[model][SessionModel]")
{
    // Rows 0 and 5 are named; rows 1 to 4 have no node and so no name.
    const model::SessionModel session (juce::ValueTree::fromXml (
        R"(<Session>
             <Scene index="5" name="Outro"/>
             <Scene index="0" name="A"/>
           </Session>)"));

    const auto scenes = session.getScenes();

    REQUIRE (scenes.size() == 2);
    CHECK (scenes[0].getIndex() == 5);
    CHECK (scenes[0].getName() == "Outro");
    CHECK (scenes[1].getIndex() == 0);
    CHECK (scenes[1].getName() == "A");
}
