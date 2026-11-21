// model::ProjectModel: the root of the document. docs/SCHEMA.md -> Project.

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/ProjectModel.h"

namespace
{

// The Routing, Timeline, Session and one clip of the schema's "Berlin test"
// example. Tracks and UiState are there because every Project has them;
// their wrappers come with later tasks.
juce::ValueTree berlinProject()
{
    return juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" name="Berlin test" ppq="960">
             <Routing>
               <Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>
               <Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>
               <PadMap>
                 <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
                 <Pad index="1" inputPitch="37" pitch="42" label="Hat"/>
               </PadMap>
             </Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="118"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
             </Timeline>
             <ClipPool>
               <Clip id="5a0c7e1b3d2f4a6988b7c6d5e4f3a2b1" name="Bass 5-step" kind="step">
                 <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000"/>
               </Clip>
             </ClipPool>
             <Tracks/>
             <Session launchQuantise="3840">
               <Scene index="0" name="A"/>
             </Session>
             <UiState activeTab="stepseq"/>
           </Project>)");
}

} // namespace

TEST_CASE ("A Project gives back what is stored", "[model][ProjectModel]")
{
    const model::ProjectModel project (berlinProject());

    CHECK (project.getSchemaVersion() == 1);
    CHECK (project.getName() == "Berlin test");
    CHECK (project.getPpq() == 960);
}

TEST_CASE ("A Project without a name or ppq has an empty name and 960", "[model][ProjectModel]")
{
    const model::ProjectModel project (juce::ValueTree::fromXml (R"(<Project schemaVersion="1"/>)"));

    CHECK (project.getName() == "");
    CHECK (project.getPpq() == 960);
}

TEST_CASE ("A Project reports a ppq other than 960 as it is stored", "[model][ProjectModel]")
{
    // Refusing it is the Validator's job (invariant 7), not the wrapper's.
    const model::ProjectModel project (juce::ValueTree::fromXml (R"(<Project schemaVersion="1" ppq="480"/>)"));

    CHECK (project.getPpq() == 480);
}

TEST_CASE ("A Project gives its Routing, Timeline and Session", "[model][ProjectModel]")
{
    const model::ProjectModel project (berlinProject());

    const auto routing = project.getRouting();
    REQUIRE (routing.getPlayers().size() == 1);
    CHECK (routing.getPlayers()[0].getName() == "Minimoog");
    REQUIRE (routing.getPerformers().size() == 1);
    CHECK (routing.getPerformers()[0].getName() == "Keyboard");
    CHECK (routing.getPadMap().getPads().size() == 2);

    const auto timeline = project.getTimeline();
    REQUIRE (timeline.getTempoEvents().size() == 1);
    CHECK (juce::exactlyEqual (timeline.getTempoEvents()[0].getBpm(), 118.0));
    REQUIRE (timeline.getTimeSigEvents().size() == 1);
    CHECK (timeline.getTimeSigEvents()[0].getNumerator() == 4);

    const auto session = project.getSession();
    CHECK (session.getLaunchQuantise() == 3840);
    REQUIRE (session.getScenes().size() == 1);
    CHECK (session.getScenes()[0].getName() == "A");
}

TEST_CASE ("A Project gives its ClipPool", "[model][ProjectModel]")
{
    const model::ProjectModel project (berlinProject());

    const auto clips = project.getClipPool().getClips();

    REQUIRE (clips.size() == 1);
    CHECK (clips[0].getName() == "Bass 5-step");
    CHECK (clips[0].getLoopLength() == 1200);
}

TEST_CASE ("A wrapper sees a later change to the document", "[model][ProjectModel]")
{
    // Wrappers are lenses, not copies (rule 6): they hold the node, not its values.
    auto tree = berlinProject();
    const model::ProjectModel project (tree);

    tree.setProperty (model::ids::name, "Renamed", nullptr);

    CHECK (project.getName() == "Renamed");
}
