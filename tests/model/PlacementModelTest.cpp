// model::PlacementModel: a reference to a clip at a position on a Track or
// an AutomationLane. docs/SCHEMA.md -> Placements / Placement.

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/PlacementModel.h"

#include <cstdint>

TEST_CASE ("A Placement gives back what is stored", "[model][PlacementModel]")
{
    // The second placement of the schema example.
    const model::PlacementModel placement (juce::ValueTree::fromXml (
        R"(<Placement id="2e4a6c8e0b1d4f3a9c7e5b3d1f0a2c4e" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"
                      start="7680" length="7680" offset="480"/>)"));

    CHECK (placement.getId() == "2e4a6c8e0b1d4f3a9c7e5b3d1f0a2c4e");
    CHECK (placement.getClipId() == "5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1");
    CHECK (placement.getStart() == 7680);
    CHECK (placement.getLength() == 7680);
    CHECK (placement.getOffset() == 480);
}

TEST_CASE ("A Placement without an offset starts the clip's loop at 0", "[model][PlacementModel]")
{
    const model::PlacementModel placement (juce::ValueTree::fromXml (
        R"(<Placement id="2e4a6c8e0b1d4f3a9c7e5b3d1f0a2c4e" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"
                      start="0" length="7680"/>)"));

    CHECK (placement.getOffset() == 0);
}

TEST_CASE ("A Placement ends at start + length", "[model][PlacementModel]")
{
    const model::PlacementModel placement (juce::ValueTree::fromXml (
        R"(<Placement id="2e4a6c8e0b1d4f3a9c7e5b3d1f0a2c4e" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"
                      start="960" length="1200" offset="480"/>)"));

    // The offset moves the phase of the loop, not the extent on the timeline.
    CHECK (placement.getEnd() == 2160);
}

TEST_CASE ("A Placement holds ticks beyond 32 bits", "[model][PlacementModel]")
{
    const model::PlacementModel placement (juce::ValueTree::fromXml (
        R"(<Placement id="2e4a6c8e0b1d4f3a9c7e5b3d1f0a2c4e" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"
                      start="5000000000" length="3000000000"/>)"));

    CHECK (placement.getStart() == std::int64_t { 5000000000 });
    CHECK (placement.getEnd() == std::int64_t { 8000000000 });
}

TEST_CASE ("A Placement reads values set in this session like loaded ones", "[model][PlacementModel]")
{
    // A command stores numbers, a file gives strings; both read the same.
    juce::ValueTree node (model::ids::Placement);
    node.setProperty (model::ids::start, juce::int64 { 1920 }, nullptr);
    node.setProperty (model::ids::length, juce::int64 { 3840 }, nullptr);

    const model::PlacementModel placement (node);

    CHECK (placement.getStart() == 1920);
    CHECK (placement.getEnd() == 5760);
}
