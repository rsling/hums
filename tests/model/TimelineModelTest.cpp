// model::TimelineModel: tempo map, time-signature map and markers as stored.
// docs/SCHEMA.md -> Timeline.

#include <catch2/catch_test_macros.hpp>

#include "model/TimelineModel.h"

TEST_CASE ("A Timeline lists each kind of child separately", "[model][TimelineModel]")
{
    const model::TimelineModel timeline (juce::ValueTree::fromXml (
        R"(<Timeline>
             <TempoEvent tick="0" bpm="118"/>
             <TimeSigEvent tick="0" numerator="4" denominator="4"/>
             <Marker tick="0" name="Intro"/>
             <TempoEvent tick="7680" bpm="124"/>
             <Marker tick="7680" name="Verse"/>
           </Timeline>)"));

    const auto tempos = timeline.getTempoEvents();
    REQUIRE (tempos.size() == 2);
    CHECK (tempos[0].getTick() == 0);
    CHECK (juce::exactlyEqual (tempos[0].getBpm(), 118.0));
    CHECK (tempos[1].getTick() == 7680);
    CHECK (juce::exactlyEqual (tempos[1].getBpm(), 124.0));

    const auto timeSigs = timeline.getTimeSigEvents();
    REQUIRE (timeSigs.size() == 1);
    CHECK (timeSigs[0].getNumerator() == 4);
    CHECK (timeSigs[0].getDenominator() == 4);

    const auto markers = timeline.getMarkers();
    REQUIRE (markers.size() == 2);
    CHECK (markers[0].getName() == "Intro");
    CHECK (markers[1].getName() == "Verse");
}

TEST_CASE ("A Timeline keeps document order and does not sort by tick", "[model][TimelineModel]")
{
    const model::TimelineModel timeline (juce::ValueTree::fromXml (
        R"(<Timeline>
             <TempoEvent tick="7680" bpm="124"/>
             <TempoEvent tick="0" bpm="118"/>
             <TimeSigEvent tick="0" numerator="4" denominator="4"/>
           </Timeline>)"));

    const auto tempos = timeline.getTempoEvents();

    REQUIRE (tempos.size() == 2);
    CHECK (tempos[0].getTick() == 7680);
    CHECK (tempos[1].getTick() == 0);
}

TEST_CASE ("A Timeline without markers has none", "[model][TimelineModel]")
{
    const model::TimelineModel timeline (juce::ValueTree::fromXml (
        R"(<Timeline>
             <TempoEvent tick="0" bpm="120"/>
             <TimeSigEvent tick="0" numerator="4" denominator="4"/>
           </Timeline>)"));

    CHECK (timeline.getMarkers().empty());
}
