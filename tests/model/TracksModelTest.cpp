// model::TracksModel: the project's tracks, and the two values computed from
// every reference to a clip: refcount and arrangement end.
// docs/SCHEMA.md -> Tracks / Track, -> ClipPool / Clip (refcount),
// -> UiState (arrangement end).

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/TracksModel.h"

#include <cstdint>

namespace
{

const juce::String bassClip { "5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" };
const juce::String drumClip { "6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" };
const juce::String sweepClip { "7c2e9a3d5f4b4c8baad9e8f7a6b5c4d3" };
const juce::String unusedClip { "8d3f0b4e6a5c4d9cbbe0f9a8b7c6d5e4" };

// The Tracks of the schema's "Berlin test" example: the bass clip placed
// twice and in a slot, the drum clip only in a slot.
juce::ValueTree berlinTracks()
{
    return juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass" channel="1">
               <Placements>
                 <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="7680"/>
                 <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="7680" length="7680" offset="480"/>
               </Placements>
               <AutomationLanes/>
               <Knobs><Knob cc="74" label="Cutoff"/></Knobs>
               <Slots><Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/></Slots>
             </Track>
             <Track id="5d7f9b1d3f5a4c2e9b7d5f3b1d2f4a6c" name="Drums" channel="10">
               <Placements/>
               <AutomationLanes/>
               <Knobs/>
               <Slots><Slot scene="0" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2"/></Slots>
             </Track>
           </Tracks>)");
}

// One track whose automation lane reaches further than its own placement.
juce::ValueTree tracksWithAutomation()
{
    return juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
               <Placements>
                 <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="3840"/>
               </Placements>
               <AutomationLanes>
                 <AutomationLane id="8d2f4b6a0c1e4a3b9d7f5c3e1a0b2d4f">
                   <Placement id="3c5e7a9c1d2f4b4a8e6c4a2e0d1f3b5c" clipId="7c2e9a3d5f4b4c8baad9e8f7a6b5c4d3" start="1920" length="1920"/>
                   <Placement id="4d6f8b0d2e3a4c5b9f7d5b3f1e2a4c6d" clipId="7c2e9a3d5f4b4c8baad9e8f7a6b5c4d3" start="7680" length="1920"/>
                 </AutomationLane>
               </AutomationLanes>
               <Knobs/>
               <Slots/>
             </Track>
           </Tracks>)");
}

} // namespace

TEST_CASE ("Tracks lists its tracks in document order", "[model][TracksModel]")
{
    const model::TracksModel tracks (berlinTracks());

    const auto list = tracks.getTracks();

    REQUIRE (list.size() == 2);
    CHECK (list[0].getName() == "Bass");
    CHECK (list[1].getName() == "Drums");
    CHECK (list[1].getChannel() == 10);
}

TEST_CASE ("Tracks skips children it does not know", "[model][TracksModel]")
{
    const model::TracksModel tracks (juce::ValueTree::fromXml (
        R"(<Tracks>
             <TrackGroup name="Synths"/>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass"><Placements/><AutomationLanes/><Knobs/><Slots/></Track>
           </Tracks>)"));

    REQUIRE (tracks.getTracks().size() == 1);
    CHECK (tracks.getTracks()[0].getName() == "Bass");
}

//==============================================================================
// Refcount

TEST_CASE ("A clip's refcount counts placements and slots", "[model][TracksModel][refcount]")
{
    // "Refcount of c1 is 3 (two placements, one slot)", docs/SCHEMA.md -> Example.
    const model::TracksModel tracks (berlinTracks());

    CHECK (tracks.getRefcount (bassClip) == 3);
}

TEST_CASE ("A clip that only sits in a slot has refcount 1", "[model][TracksModel][refcount]")
{
    const model::TracksModel tracks (berlinTracks());

    CHECK (tracks.getRefcount (drumClip) == 1);
}

TEST_CASE ("A clip nothing references has refcount 0", "[model][TracksModel][refcount]")
{
    const model::TracksModel tracks (berlinTracks());

    CHECK (tracks.getRefcount (unusedClip) == 0);
    CHECK (model::TracksModel (juce::ValueTree::fromXml ("<Tracks/>")).getRefcount (bassClip) == 0);
}

TEST_CASE ("A clip's refcount counts placements on automation lanes", "[model][TracksModel][refcount]")
{
    const model::TracksModel tracks (tracksWithAutomation());

    CHECK (tracks.getRefcount (sweepClip) == 2);
    CHECK (tracks.getRefcount (bassClip) == 1);
}

TEST_CASE ("A clip's refcount adds up over tracks", "[model][TracksModel][refcount]")
{
    const model::TracksModel tracks (juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
               <Placements>
                 <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="3840"/>
               </Placements>
               <AutomationLanes/><Knobs/>
               <Slots>
                 <Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>
                 <Slot scene="1" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/>
               </Slots>
             </Track>
             <Track id="5d7f9b1d3f5a4c2e9b7d5f3b1d2f4a6c">
               <Placements>
                 <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="3840"/>
               </Placements>
               <AutomationLanes/><Knobs/>
               <Slots><Slot scene="0" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1"/></Slots>
             </Track>
           </Tracks>)"));

    CHECK (tracks.getRefcount (bassClip) == 5);
}

TEST_CASE ("A refcount is computed, so it follows the document", "[model][TracksModel][refcount]")
{
    auto tree = berlinTracks();
    const model::TracksModel tracks (tree);
    REQUIRE (tracks.getRefcount (bassClip) == 3);

    auto bassSlots = tree.getChild (0).getChildWithName (model::ids::Slots);
    bassSlots.removeChild (0, nullptr);

    CHECK (tracks.getRefcount (bassClip) == 2);
}

//==============================================================================
// Arrangement end

TEST_CASE ("The arrangement ends where the last placement ends", "[model][TracksModel][arrangementEnd]")
{
    const model::TracksModel tracks (berlinTracks());

    const auto end = tracks.getArrangementEnd();

    REQUIRE (end.has_value());
    CHECK (*end == 15360);
}

TEST_CASE ("The arrangement end is the largest end, not the last child's", "[model][TracksModel][arrangementEnd]")
{
    // The later placement is stored first, and it sits on the first track.
    const model::TracksModel tracks (juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
               <Placements>
                 <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="9600" length="960"/>
                 <Placement id="2b4d6f8b0c1e4a3f9d7b5f3d1c2e4a6b" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="0" length="3840"/>
               </Placements>
               <AutomationLanes/><Knobs/><Slots/>
             </Track>
             <Track id="5d7f9b1d3f5a4c2e9b7d5f3b1d2f4a6c">
               <Placements>
                 <Placement id="3c5e7a9c1d2f4b4a8e6c4a2e0d1f3b5c" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" start="0" length="7680"/>
               </Placements>
               <AutomationLanes/><Knobs/><Slots/>
             </Track>
           </Tracks>)"));

    REQUIRE (tracks.getArrangementEnd().has_value());
    CHECK (*tracks.getArrangementEnd() == 10560);
}

TEST_CASE ("An automation lane placement counts towards the arrangement end", "[model][TracksModel][arrangementEnd]")
{
    const model::TracksModel tracks (tracksWithAutomation());

    REQUIRE (tracks.getArrangementEnd().has_value());
    CHECK (*tracks.getArrangementEnd() == 9600);
}

TEST_CASE ("Slots never count towards the arrangement end", "[model][TracksModel][arrangementEnd]")
{
    // Only slots: there is nothing to export.
    const model::TracksModel tracks (juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="5d7f9b1d3f5a4c2e9b7d5f3b1d2f4a6c">
               <Placements/><AutomationLanes/><Knobs/>
               <Slots><Slot scene="0" clipId="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2"/></Slots>
             </Track>
           </Tracks>)"));

    CHECK_FALSE (tracks.getArrangementEnd().has_value());
}

TEST_CASE ("Without any track there is no arrangement end", "[model][TracksModel][arrangementEnd]")
{
    const model::TracksModel tracks (juce::ValueTree::fromXml ("<Tracks/>"));

    CHECK_FALSE (tracks.getArrangementEnd().has_value());
}

TEST_CASE ("The arrangement end holds ticks beyond 32 bits", "[model][TracksModel][arrangementEnd]")
{
    const model::TracksModel tracks (juce::ValueTree::fromXml (
        R"(<Tracks>
             <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b">
               <Placements>
                 <Placement id="1a3c5e7a9b0d4f2e8c6a4e2c0b1d3f5a" clipId="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" start="5000000000" length="3840"/>
               </Placements>
               <AutomationLanes/><Knobs/><Slots/>
             </Track>
           </Tracks>)"));

    REQUIRE (tracks.getArrangementEnd().has_value());
    CHECK (*tracks.getArrangementEnd() == std::int64_t { 5000003840 });
}
