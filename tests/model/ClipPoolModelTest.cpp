// model::ClipPoolModel: where all musical content lives.
// docs/SCHEMA.md -> ClipPool / Clip.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipPoolModel.h"

namespace
{

juce::ValueTree poolOfTwo()
{
    return juce::ValueTree::fromXml (
        R"(<ClipPool>
             <Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" name="Bass 5-step" kind="step"><Grid stepCount="5"/></Clip>
             <Clip id="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" name="Lead" kind="event" loopLength="3840"/>
           </ClipPool>)");
}

} // namespace

TEST_CASE ("A ClipPool lists its clips in document order", "[model][ClipPoolModel]")
{
    const model::ClipPoolModel pool (poolOfTwo());

    const auto clips = pool.getClips();

    REQUIRE (clips.size() == 2);
    CHECK (clips[0].getName() == "Bass 5-step");
    CHECK (clips[0].getKind() == model::ClipModel::Kind::step);
    CHECK (clips[1].getName() == "Lead");
    CHECK (clips[1].getKind() == model::ClipModel::Kind::event);
}

TEST_CASE ("An empty ClipPool has no clips", "[model][ClipPoolModel]")
{
    const model::ClipPoolModel pool (juce::ValueTree::fromXml ("<ClipPool/>"));

    CHECK (pool.getClips().empty());
    CHECK_FALSE (pool.findClip ("5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1").has_value());
}

TEST_CASE ("A ClipPool finds a clip by its id", "[model][ClipPoolModel]")
{
    const model::ClipPoolModel pool (poolOfTwo());

    const auto lead = pool.findClip ("6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2");

    REQUIRE (lead.has_value());
    CHECK (lead->getName() == "Lead");
}

TEST_CASE ("A ClipPool finds nothing for an id it does not hold", "[model][ClipPoolModel]")
{
    const model::ClipPoolModel pool (poolOfTwo());

    CHECK_FALSE (pool.findClip ("00000000000000000000000000000000").has_value());
    CHECK_FALSE (pool.findClip ("").has_value());
}

TEST_CASE ("A ClipPool skips children it does not know", "[model][ClipPoolModel]")
{
    const model::ClipPoolModel pool (juce::ValueTree::fromXml (
        R"(<ClipPool>
             <Folder name="Drums"/>
             <Clip id="6b1d8f2c4e3a4b7a99c8d7e6f5a4b3c2" kind="event" loopLength="960"/>
           </ClipPool>)"));

    REQUIRE (pool.getClips().size() == 1);
    CHECK (pool.getClips()[0].getLoopLength() == 960);
}
