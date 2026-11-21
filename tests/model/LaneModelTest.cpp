// model::LaneModel: a monophonic row of Cells. docs/SCHEMA.md -> Step clip: Grid.

#include <catch2/catch_test_macros.hpp>

#include "model/LaneModel.h"

TEST_CASE ("A drum Lane has a name and a fixed pitch", "[model][LaneModel]")
{
    const model::LaneModel lane (juce::ValueTree::fromXml (
        R"(<Lane id="7c3e1a9b5d2f4e6080a1b2c3d4e5f607" name="Hat" fixedPitch="42"/>)"));

    CHECK (lane.getId() == "7c3e1a9b5d2f4e6080a1b2c3d4e5f607");
    CHECK (lane.getName() == "Hat");
    REQUIRE (lane.getFixedPitch().has_value());
    CHECK (*lane.getFixedPitch() == 42);
}

TEST_CASE ("A melodic Lane has no fixed pitch and no name", "[model][LaneModel]")
{
    const model::LaneModel lane (juce::ValueTree::fromXml (
        R"(<Lane id="7c3e1a9b5d2f4e6080a1b2c3d4e5f607"/>)"));

    CHECK (lane.getName() == "");
    CHECK_FALSE (lane.getFixedPitch().has_value());
}

TEST_CASE ("A fixed pitch of 0 is a fixed pitch", "[model][LaneModel]")
{
    // Pitch 0 is a valid note, so "no fixed pitch" cannot be told by the value.
    const model::LaneModel lane (juce::ValueTree::fromXml (
        R"(<Lane id="7c3e1a9b5d2f4e6080a1b2c3d4e5f607" fixedPitch="0"/>)"));

    REQUIRE (lane.getFixedPitch().has_value());
    CHECK (*lane.getFixedPitch() == 0);
}

TEST_CASE ("A Lane lists its cells in column order", "[model][LaneModel]")
{
    const model::LaneModel lane (juce::ValueTree::fromXml (
        R"(<Lane id="7c3e1a9b5d2f4e6080a1b2c3d4e5f607">
             <Cell pitch="36" on="1"/>
             <Cell pitch="43"/>
             <Cell pitch="48" on="1"/>
           </Lane>)"));

    const auto cells = lane.getCells();

    REQUIRE (cells.size() == 3);
    CHECK (cells[0].getPitch() == 36);
    CHECK (cells[1].getPitch() == 43);
    CHECK (cells[2].getPitch() == 48);
}

TEST_CASE ("A child a Lane does not know does not shift the columns", "[model][LaneModel]")
{
    // A file from a newer build may carry such a child; the column of a cell
    // is its position among the Cell children only.
    const model::LaneModel lane (juce::ValueTree::fromXml (
        R"(<Lane id="7c3e1a9b5d2f4e6080a1b2c3d4e5f607">
             <Cell pitch="36"/>
             <Ratchet count="3"/>
             <Cell pitch="43"/>
           </Lane>)"));

    const auto cells = lane.getCells();

    REQUIRE (cells.size() == 2);
    CHECK (cells[1].getPitch() == 43);
}
