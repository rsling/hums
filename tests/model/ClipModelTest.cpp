// model::ClipModel: a unit of musical content in the ClipPool.
// docs/SCHEMA.md -> ClipPool / Clip.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipModel.h"
#include "model/Ids.h"

namespace
{

std::int64_t quantiseGridOf (const char* quantise)
{
    auto clip = juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="event" loopLength="3840"/>)");
    clip.setProperty (model::ids::quantise, quantise, nullptr);

    return model::ClipModel (clip).getQuantiseGrid();
}

} // namespace

TEST_CASE ("An event Clip gives back what is stored", "[model][ClipModel]")
{
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" name="Lead" kind="event" loopLength="3840"
                 quantise="1/8" colour="#FF336699"/>)"));

    CHECK (clip.getId() == "0a1b2c3d4e5f40718293a4b5c6d7e8f9");
    CHECK (clip.getName() == "Lead");
    CHECK (clip.getKind() == model::ClipModel::Kind::event);
    CHECK (clip.getLoopLength() == 3840);
    CHECK (clip.getQuantise() == "1/8");
    CHECK (clip.getColour() == "#FF336699");
}

TEST_CASE ("A Clip without name, colour and quantise has none", "[model][ClipModel]")
{
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="event" loopLength="960"/>)"));

    CHECK (clip.getName() == "");
    CHECK (clip.getColour() == "");
    CHECK (clip.getQuantise() == "");
    CHECK (clip.getQuantiseGrid() == 0);
}

TEST_CASE ("Each quantise setting is a grid in ticks", "[model][ClipModel]")
{
    CHECK (quantiseGridOf ("") == 0);
    CHECK (quantiseGridOf ("1/2") == 1920);
    CHECK (quantiseGridOf ("1/4") == 960);
    CHECK (quantiseGridOf ("1/8") == 480);
    CHECK (quantiseGridOf ("1/2T") == 1280);
    CHECK (quantiseGridOf ("1/4T") == 640);
    CHECK (quantiseGridOf ("1/8T") == 320);
}

TEST_CASE ("An event Clip lists notes and events separately, each in child order", "[model][ClipModel]")
{
    // Out of tick order and interleaved on purpose: child order is the
    // tie-breaker at equal ticks and is never re-sorted.
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="event" loopLength="3840">
             <Events>
               <Event tick="0" kind="cc" a="0" b="1"/>
               <Note tick="960" length="240" pitch="62"/>
               <Event tick="0" kind="programChange" a="5"/>
               <Note tick="0" length="240" pitch="60"/>
             </Events>
           </Clip>)"));

    const auto notes = clip.getNotes();
    REQUIRE (notes.size() == 2);
    CHECK (notes[0].getPitch() == 62);
    CHECK (notes[1].getPitch() == 60);

    const auto events = clip.getEvents();
    REQUIRE (events.size() == 2);
    CHECK (events[0].getKind() == model::EventModel::Kind::cc);
    CHECK (events[1].getKind() == model::EventModel::Kind::programChange);
}

TEST_CASE ("An event Clip's loop length is the stored one, whatever its content", "[model][ClipModel]")
{
    // Content past the loop length is kept and never played; it does not
    // lengthen the loop.
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="event" loopLength="960">
             <Events>
               <Note tick="1920" length="480" pitch="60"/>
             </Events>
           </Clip>)"));

    CHECK (clip.getLoopLength() == 960);
    CHECK (clip.getNotes().size() == 1);
}

TEST_CASE ("A step Clip's loop length comes from its Grid", "[model][ClipModel]")
{
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" name="Bass 5-step" kind="step">
             <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000">
               <Lane id="1e4b7a0d3c6f49528b5e8a1d4c7f0a3b"/>
             </Grid>
           </Clip>)"));

    CHECK (clip.getKind() == model::ClipModel::Kind::step);
    CHECK (clip.getLoopLength() == 1200);
    CHECK (clip.getGrid().getStepCount() == 8);
    CHECK (clip.getGrid().getLanes().size() == 1);
}

TEST_CASE ("A step Clip has no notes and no events", "[model][ClipModel]")
{
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="step">
             <Grid/>
           </Clip>)"));

    CHECK (clip.getNotes().empty());
    CHECK (clip.getEvents().empty());
}

TEST_CASE ("An event Clip with no Events child yet has no notes and no events", "[model][ClipModel]")
{
    const model::ClipModel clip (juce::ValueTree::fromXml (
        R"(<Clip id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" kind="event" loopLength="960"/>)"));

    CHECK (clip.getNotes().empty());
    CHECK (clip.getEvents().empty());
}
