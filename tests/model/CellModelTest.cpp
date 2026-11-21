// model::CellModel: one step in one Lane. docs/SCHEMA.md -> Step clip: Grid.

#include <catch2/catch_test_macros.hpp>

#include "model/CellModel.h"

TEST_CASE ("A Cell gives back what is stored", "[model][CellModel]")
{
    const model::CellModel cell (juce::ValueTree::fromXml (
        R"(<Cell pitch="43" on="1" velocity="110" gate="1.2"/>)"));

    CHECK (cell.getPitch() == 43);
    CHECK (cell.isOn());
    CHECK (cell.getVelocity() == 110);
    CHECK (juce::exactlyEqual (cell.getGate(), 1.2));
}

TEST_CASE ("An empty Cell is an off cell with the schema's defaults", "[model][CellModel]")
{
    const model::CellModel cell (juce::ValueTree::fromXml ("<Cell/>"));

    CHECK (cell.getPitch() == 60);
    CHECK_FALSE (cell.isOn());
    CHECK (cell.getVelocity() == 100);
    CHECK (juce::exactlyEqual (cell.getGate(), 0.5));
    CHECK (cell.getCcValues().empty());
}

TEST_CASE ("A Cell lists its CC values in document order", "[model][CellModel]")
{
    const model::CellModel cell (juce::ValueTree::fromXml (
        R"(<Cell pitch="41" on="1">
             <CcValue cc="74" value="90"/>
             <CcValue cc="71" value="20"/>
           </Cell>)"));

    const auto ccValues = cell.getCcValues();

    REQUIRE (ccValues.size() == 2);
    CHECK (ccValues[0].getCc() == 74);
    CHECK (ccValues[0].getValue() == 90);
    CHECK (ccValues[1].getCc() == 71);
    CHECK (ccValues[1].getValue() == 20);
}

TEST_CASE ("An off Cell still has its CC values", "[model][CellModel]")
{
    // A rest still moves the CV row: the values are sent for off cells too.
    const model::CellModel cell (juce::ValueTree::fromXml (
        R"(<Cell on="0">
             <CcValue cc="74" value="33"/>
           </Cell>)"));

    CHECK_FALSE (cell.isOn());
    REQUIRE (cell.getCcValues().size() == 1);
    CHECK (cell.getCcValues()[0].getValue() == 33);
}
