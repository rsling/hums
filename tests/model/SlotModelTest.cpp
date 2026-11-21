// model::SlotModel: a launchable clip reference on a Track. docs/SCHEMA.md -> Slots / Slot.

#include <catch2/catch_test_macros.hpp>

#include "model/SlotModel.h"

TEST_CASE ("A Slot gives back what is stored", "[model][SlotModel]")
{
    const model::SlotModel slot (juce::ValueTree::fromXml (
        R"(<Slot scene="3" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>)"));

    CHECK (slot.getScene() == 3);
    CHECK (slot.getClipId() == "5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1");
}

TEST_CASE ("A Slot in the first scene has scene 0", "[model][SlotModel]")
{
    const model::SlotModel slot (juce::ValueTree::fromXml (
        R"(<Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>)"));

    CHECK (slot.getScene() == 0);
}
