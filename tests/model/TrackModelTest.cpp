// model::TrackModel: a lane in the arrangement. docs/SCHEMA.md -> Tracks / Track.

#include <catch2/catch_test_macros.hpp>

#include "model/TrackModel.h"

namespace
{

// The Bass track of the schema's "Berlin test" example, with an automation
// lane, a colour, mute and solo added.
juce::ValueTree bassTrack()
{
    return juce::ValueTree::fromXml (
        R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass" colour="#FF336699"
                  playerId="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" channel="3"
                  performerId="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" mute="1" solo="1">
             <Placements>
               <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680"/>
               <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="7680" length="7680" offset="480"/>
             </Placements>
             <AutomationLanes>
               <AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f" name="Sweep">
                 <Placement id="3c5e7a9c1d2f4b4a8e6c4a2e0d1f3b5c" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="3840" length="1920"/>
               </AutomationLane>
               <AutomationLane id="9e3a5c7b1d2f4b4c8e6a4d2f0b1c3e5a" name="Resonance"/>
             </AutomationLanes>
             <Knobs>
               <Knob cc="74" label="Cutoff"/>
               <Knob cc="71" label="Res"/>
             </Knobs>
             <Slots>
               <Slot scene="2" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>
               <Slot scene="0" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2"/>
             </Slots>
           </Track>)");
}

juce::ValueTree bareTrack()
{
    return juce::ValueTree::fromXml (
        R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
             <Placements/>
             <AutomationLanes/>
             <Knobs/>
             <Slots/>
           </Track>)");
}

} // namespace

TEST_CASE ("A Track gives back what is stored", "[model][TrackModel]")
{
    const model::TrackModel track (bassTrack());

    CHECK (track.getId() == "4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b");
    CHECK (track.getName() == "Bass");
    CHECK (track.getColour() == "#FF336699");
    CHECK (track.getPlayerId() == "9b1d6f3a2c4e4f708192a3b4c5d6e7f8");
    CHECK (track.getChannel() == 3);
    CHECK (track.getPerformerId() == "3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1");
    CHECK (track.isMuted() == true);
    CHECK (track.isSoloed() == true);
}

TEST_CASE ("A bare Track is unassigned, on channel 1, audible and empty", "[model][TrackModel]")
{
    const model::TrackModel track (bareTrack());

    CHECK (track.getName() == "");
    CHECK (track.getColour() == "");
    CHECK (track.getPlayerId() == "");
    CHECK (track.getChannel() == 1);
    CHECK (track.getPerformerId() == "");
    CHECK (track.isMuted() == false);
    CHECK (track.isSoloed() == false);

    CHECK (track.getPlacements().empty());
    CHECK (track.getAutomationLanes().empty());
    CHECK (track.getKnobs().empty());
    CHECK (track.getSlots().empty());
}

TEST_CASE ("A Track lists its own placements, not those of its automation lanes", "[model][TrackModel]")
{
    const model::TrackModel track (bassTrack());

    const auto placements = track.getPlacements();

    REQUIRE (placements.size() == 2);
    CHECK (placements[0].getStart() == 0);
    CHECK (placements[1].getStart() == 7680);
    CHECK (placements[1].getOffset() == 480);
}

TEST_CASE ("A Track lists its automation lanes in document order", "[model][TrackModel]")
{
    // The order is semantic: a later lane overrides an earlier one.
    const model::TrackModel track (bassTrack());

    const auto lanes = track.getAutomationLanes();

    REQUIRE (lanes.size() == 2);
    CHECK (lanes[0].getName() == "Sweep");
    REQUIRE (lanes[0].getPlacements().size() == 1);
    CHECK (lanes[0].getPlacements()[0].getStart() == 3840);
    CHECK (lanes[1].getName() == "Resonance");
    CHECK (lanes[1].getPlacements().empty());
}

TEST_CASE ("A Track lists its knobs in document order", "[model][TrackModel]")
{
    // Left to right, not sorted by controller number.
    const model::TrackModel track (bassTrack());

    const auto knobs = track.getKnobs();

    REQUIRE (knobs.size() == 2);
    CHECK (knobs[0].getCc() == 74);
    CHECK (knobs[0].getLabel() == "Cutoff");
    CHECK (knobs[1].getCc() == 71);
}

TEST_CASE ("A Track lists its slots in document order", "[model][TrackModel]")
{
    // Stored out of scene order on purpose: child order is never re-sorted.
    const model::TrackModel track (bassTrack());

    const auto slots = track.getSlots();

    REQUIRE (slots.size() == 2);
    CHECK (slots[0].getScene() == 2);
    CHECK (slots[1].getScene() == 0);
}

TEST_CASE ("A Track skips children it does not know", "[model][TrackModel]")
{
    // A file from a newer build may carry children this build has no name
    // for, in the track or in one of its four containers.
    const model::TrackModel track (juce::ValueTree::fromXml (
        R"(<Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
             <Transforms><Arpeggiator rate="240"/></Transforms>
             <Placements>
               <Gap start="0"/>
               <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="960" length="7680"/>
             </Placements>
             <AutomationLanes><Group name="Filter"/></AutomationLanes>
             <Knobs><Fader cc="7"/><Knob cc="74"/></Knobs>
             <Slots><Slot scene="1" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/><Follow scene="1"/></Slots>
           </Track>)"));

    REQUIRE (track.getPlacements().size() == 1);
    CHECK (track.getPlacements()[0].getStart() == 960);
    CHECK (track.getAutomationLanes().empty());
    REQUIRE (track.getKnobs().size() == 1);
    CHECK (track.getKnobs()[0].getCc() == 74);
    REQUIRE (track.getSlots().size() == 1);
    CHECK (track.getSlots()[0].getScene() == 1);
}
