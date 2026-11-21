// model::AutomationLaneModel: a track-level override layer for CCs.
// docs/SCHEMA.md -> AutomationLanes / AutomationLane.

#include <catch2/catch_test_macros.hpp>

#include "model/AutomationLaneModel.h"

TEST_CASE ("An AutomationLane gives back what is stored", "[model][AutomationLaneModel]")
{
    const model::AutomationLaneModel lane (juce::ValueTree::fromXml (
        R"(<AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f" name="Filter sweep"/>)"));

    CHECK (lane.getId() == "8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f");
    CHECK (lane.getName() == "Filter sweep");
    CHECK (lane.getPlacements().empty());
}

TEST_CASE ("An AutomationLane without a name has an empty one", "[model][AutomationLaneModel]")
{
    const model::AutomationLaneModel lane (juce::ValueTree::fromXml (
        R"(<AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f"/>)"));

    CHECK (lane.getName() == "");
}

TEST_CASE ("An AutomationLane lists its placements in document order", "[model][AutomationLaneModel]")
{
    // Stored out of time order on purpose: child order is never re-sorted.
    const model::AutomationLaneModel lane (juce::ValueTree::fromXml (
        R"(<AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f">
             <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="7680" length="3840"/>
             <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="0" length="3840"/>
           </AutomationLane>)"));

    const auto placements = lane.getPlacements();

    REQUIRE (placements.size() == 2);
    CHECK (placements[0].getStart() == 7680);
    CHECK (placements[1].getStart() == 0);
}

TEST_CASE ("An AutomationLane skips children it does not know", "[model][AutomationLaneModel]")
{
    const model::AutomationLaneModel lane (juce::ValueTree::fromXml (
        R"(<AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f">
             <Curve shape="ramp"/>
             <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="960" length="3840"/>
           </AutomationLane>)"));

    REQUIRE (lane.getPlacements().size() == 1);
    CHECK (lane.getPlacements()[0].getStart() == 960);
}
