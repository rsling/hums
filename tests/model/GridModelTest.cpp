// model::GridModel: the content of a step clip. docs/SCHEMA.md -> Step clip: Grid.

#include <catch2/catch_test_macros.hpp>

#include "model/GridModel.h"

TEST_CASE ("A Grid gives back what is stored", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepLength="120" stepCount="8" resetAt="6" skipMask="00010000"/>)"));

    CHECK (grid.getStepLength() == 120);
    CHECK (grid.getStepCount() == 8);
    CHECK (grid.getResetAt() == 6);
    CHECK (grid.getSkipMask() == "00010000");
}

TEST_CASE ("An empty Grid is sixteen sixteenths, all playing", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml ("<Grid/>"));

    CHECK (grid.getStepLength() == 240);
    CHECK (grid.getStepCount() == 16);
    CHECK (grid.getResetAt() == 16);
    CHECK (grid.getSkipMask() == "0000000000000000");
    CHECK (grid.getAudibleColumnCount() == 16);
    CHECK (grid.getLoopLength() == 3840);
    CHECK (grid.getLanes().empty());
}

TEST_CASE ("Without a stored resetAt and skipMask both follow the step count", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (R"(<Grid stepCount="5"/>)"));

    CHECK (grid.getResetAt() == 5);
    CHECK (grid.getSkipMask() == "00000");
    CHECK (grid.getLoopLength() == 1200);
}

TEST_CASE ("A Grid tells which columns are skipped", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepCount="4" skipMask="0101"/>)"));

    CHECK_FALSE (grid.isColumnSkipped (0));
    CHECK (grid.isColumnSkipped (1));
    CHECK_FALSE (grid.isColumnSkipped (2));
    CHECK (grid.isColumnSkipped (3));
}

TEST_CASE ("The loop of the schema's five-step bass is 1200 ticks", "[model][GridModel]")
{
    // Eight columns, column 3 skipped, loop ends after column 5: columns
    // 0, 1, 2, 4, 5 take time. Five sixteenths.
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000"/>)"));

    CHECK (grid.getAudibleColumnCount() == 5);
    CHECK (grid.getLoopLength() == 1200);
}

TEST_CASE ("A skipped column at or past resetAt does not shorten the loop", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepLength="240" stepCount="8" resetAt="4" skipMask="00001100"/>)"));

    CHECK (grid.getAudibleColumnCount() == 4);
    CHECK (grid.getLoopLength() == 960);
}

TEST_CASE ("The loop length follows the step length", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepLength="3840" stepCount="64"/>)"));

    // The largest grid the schema allows.
    CHECK (grid.getLoopLength() == 245760);
}

TEST_CASE ("A Grid lists its lanes top to bottom", "[model][GridModel]")
{
    const model::GridModel grid (juce::ValueTree::fromXml (
        R"(<Grid stepCount="2">
             <Lane id="1e4b7a0d3c6f49528b5e8a1d4c7f0a3b" name="Kick" fixedPitch="36"><Cell on="1"/><Cell/></Lane>
             <Lane id="2f5c8b1e4d7a4a639c6f9b2e5d8a1b4c" name="Hat" fixedPitch="42"><Cell/><Cell on="1"/></Lane>
           </Grid>)"));

    const auto lanes = grid.getLanes();

    REQUIRE (lanes.size() == 2);
    CHECK (lanes[0].getName() == "Kick");
    CHECK (lanes[1].getName() == "Hat");
    REQUIRE (lanes[1].getCells().size() == 2);
    CHECK (lanes[1].getCells()[1].isOn());
}
