// render::SnapshotBuilder: document -> Snapshot (docs/RENDERING.md ->
// Pipeline). An event clip becomes a ClipTemplate, a track's placements its
// TrackPlan, the Timeline's tempos a TempoMap.
//
// Every document here is held to the Validator first: the builder is only
// ever handed a valid one.

#include <catch2/catch_test_macros.hpp>

#include "model/ClipModel.h"
#include "model/ProjectModel.h"
#include "model/TimelineModel.h"
#include "model/Validator.h"
#include "render/SnapshotBuilder.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using render::ClipTemplate;
using render::SnapshotBuilder;

namespace
{

// A valid project around the given children of Timeline, ClipPool and Tracks.
juce::ValueTree projectWith (const juce::String& timeline, const juce::String& clips, const juce::String& tracks)
{
    const auto project = juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" ppq="960"><Routing><PadMap/></Routing><Timeline>)" + timeline
        + "</Timeline><ClipPool>" + clips
        + "</ClipPool><Tracks>" + tracks
        + "</Tracks><Session/><UiState/></Project>");

    REQUIRE (project.isValid());
    REQUIRE (model::Validator::validate (project).empty());
    return project;
}

const char* const oneTempo = R"(<TempoEvent tick="0" bpm="120"/><TimeSigEvent tick="0" numerator="4" denominator="4"/>)";

// The template of one event clip, given its loop length and its content.
ClipTemplate templateOfEventClip (const juce::String& loopLength, const juce::String& content)
{
    const auto project = projectWith (
        oneTempo,
        R"(<Clip id="11111111111141118111111111111111" kind="event" loopLength=")" + loopLength + R"("><Events>)" + content + "</Events></Clip>",
        "");

    return SnapshotBuilder::templateOf (model::ProjectModel (project).getClipPool().getClips().front());
}

} // namespace

TEST_CASE ("An event clip's template holds its notes with their stored values", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfEventClip ("3840", R"(
        <Note tick="0" length="480" pitch="60" velocity="90" offVelocity="10"/>
        <Note tick="960" length="240" pitch="64"/>)");

    CHECK (clipTemplate.loopLength == 3840);
    REQUIRE (clipTemplate.notes.size() == 2);

    CHECK (clipTemplate.notes[0].tick == 0);
    CHECK (clipTemplate.notes[0].length == 480);
    CHECK (clipTemplate.notes[0].pitch == 60);
    CHECK (clipTemplate.notes[0].velocity == 90);
    CHECK (clipTemplate.notes[0].offVelocity == 10);

    // The defaults of the schema: velocity 100, off velocity 64.
    CHECK (clipTemplate.notes[1].tick == 960);
    CHECK (clipTemplate.notes[1].pitch == 64);
    CHECK (clipTemplate.notes[1].velocity == 100);
    CHECK (clipTemplate.notes[1].offVelocity == 64);

    CHECK (clipTemplate.events.empty());
    CHECK (clipTemplate.sysexBytes.empty());
}

TEST_CASE ("A disabled note and a disabled event are not in the template", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfEventClip ("3840", R"(
        <Note tick="0" length="480" pitch="60" enabled="0"/>
        <Note tick="0" length="480" pitch="62"/>
        <Event tick="0" kind="cc" a="74" b="1" enabled="0"/>
        <Event tick="0" kind="cc" a="74" b="2"/>)");

    REQUIRE (clipTemplate.notes.size() == 1);
    CHECK (clipTemplate.notes[0].pitch == 62);

    REQUIRE (clipTemplate.events.size() == 1);
    CHECK (clipTemplate.events[0].b == 2);
}

TEST_CASE ("Content at or past the loop length is not in the template", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfEventClip ("960", R"(
        <Note tick="959" length="10" pitch="60"/>
        <Note tick="960" length="10" pitch="61"/>
        <Note tick="5000" length="10" pitch="62"/>
        <Event tick="959" kind="cc" a="1" b="1"/>
        <Event tick="960" kind="cc" a="1" b="2"/>
        <Event tick="5000" kind="cc" a="1" b="3"/>)");

    REQUIRE (clipTemplate.notes.size() == 1);
    CHECK (clipTemplate.notes[0].pitch == 60);

    REQUIRE (clipTemplate.events.size() == 1);
    CHECK (clipTemplate.events[0].b == 1);
}

TEST_CASE ("A note that begins inside the loop keeps its full length across the loop's end", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfEventClip ("960", R"(<Note tick="720" length="2000" pitch="60"/>)");

    REQUIRE (clipTemplate.notes.size() == 1);
    CHECK (clipTemplate.notes[0].tick == 720);
    CHECK (clipTemplate.notes[0].length == 2000);
}

TEST_CASE ("A template's notes are in tick order, and in child order at one tick", "[render][SnapshotBuilder]")
{
    // Child order is not tick order: the document is never sorted.
    const auto clipTemplate = templateOfEventClip ("3840", R"(
        <Note tick="960" length="10" pitch="70"/>
        <Note tick="0" length="10" pitch="62"/>
        <Note tick="960" length="10" pitch="60"/>
        <Note tick="0" length="10" pitch="61"/>
        <Note tick="480" length="10" pitch="50"/>)");

    std::vector<int> pitches;
    std::vector<std::int64_t> ticks;

    for (const auto& note : clipTemplate.notes)
    {
        pitches.push_back (note.pitch);
        ticks.push_back (note.tick);
    }

    CHECK (ticks == std::vector<std::int64_t> { 0, 0, 480, 960, 960 });
    CHECK (pitches == std::vector<int> { 62, 61, 50, 70, 60 });
}

// The template of one event clip with a quantise setting.
namespace
{

ClipTemplate templateOfQuantisedClip (const juce::String& quantise, const juce::String& loopLength, const juce::String& content)
{
    const auto project = projectWith (
        oneTempo,
        R"(<Clip id="11111111111141118111111111111111" kind="event" loopLength=")" + loopLength + R"(" quantise=")" + quantise
            + R"("><Events>)" + content + "</Events></Clip>",
        "");

    return SnapshotBuilder::templateOf (model::ProjectModel (project).getClipPool().getClips().front());
}

} // namespace

TEST_CASE ("A quantised clip's notes stand at their quantised onsets with their stored lengths", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfQuantisedClip ("1/8", "1920", R"(
        <Note tick="10" length="200" pitch="60"/>
        <Note tick="250" length="100" pitch="62"/>
        <Note tick="1000" length="2000" pitch="65"/>)");

    REQUIRE (clipTemplate.notes.size() == 3);
    CHECK (clipTemplate.notes[0].tick == 0);
    CHECK (clipTemplate.notes[0].length == 200);
    CHECK (clipTemplate.notes[1].tick == 480);
    CHECK (clipTemplate.notes[1].length == 100);
    CHECK (clipTemplate.notes[2].tick == 960);
    CHECK (clipTemplate.notes[2].length == 2000);
}

TEST_CASE ("Each quantise setting has its grid in the template", "[render][SnapshotBuilder]")
{
    // A note one tick before the second line of each grid goes onto it.
    const auto onsetWith = [] (const juce::String& quantise, const juce::String& storedTick) {
        const auto clipTemplate = templateOfQuantisedClip (quantise, "7680", R"(<Note tick=")" + storedTick + R"(" length="10" pitch="60"/>)");
        REQUIRE (clipTemplate.notes.size() == 1);
        return clipTemplate.notes[0].tick;
    };

    CHECK (onsetWith ("1/2", "1919") == 1920);
    CHECK (onsetWith ("1/4", "959") == 960);
    CHECK (onsetWith ("1/8", "479") == 480);
    CHECK (onsetWith ("1/2T", "1279") == 1280);
    CHECK (onsetWith ("1/4T", "639") == 640);
    CHECK (onsetWith ("1/8T", "319") == 320);
    CHECK (onsetWith ("", "319") == 319);
}

TEST_CASE ("Quantising moves no event", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfQuantisedClip ("1/8", "1920", R"(
        <Event tick="250" kind="cc" a="7" b="90"/>
        <Event tick="1900" kind="programChange" a="5"/>)");

    REQUIRE (clipTemplate.events.size() == 2);
    CHECK (clipTemplate.events[0].tick == 250);
    CHECK (clipTemplate.events[1].tick == 1900);
}

TEST_CASE ("A quantised onset near the loop point wraps to tick 0, and the template is in the order of the quantised onsets", "[render][SnapshotBuilder]")
{
    // A loop of 1000 is no multiple of the grid of 480: 990 is nearer to the
    // loop point than to the line at 960.
    const auto clipTemplate = templateOfQuantisedClip ("1/8", "1000", R"(
        <Note tick="700" length="50" pitch="67"/>
        <Note tick="990" length="100" pitch="72"/>
        <Note tick="0" length="50" pitch="60"/>)");

    REQUIRE (clipTemplate.notes.size() == 3);
    // Both at tick 0, in child order: the wrapped note first.
    CHECK (clipTemplate.notes[0].tick == 0);
    CHECK (clipTemplate.notes[0].pitch == 72);
    CHECK (clipTemplate.notes[0].length == 100);
    CHECK (clipTemplate.notes[1].tick == 0);
    CHECK (clipTemplate.notes[1].pitch == 60);
    CHECK (clipTemplate.notes[2].tick == 480);
    CHECK (clipTemplate.notes[2].pitch == 67);
}

TEST_CASE ("Notes quantised onto one tick stay in child order, whatever their stored ticks", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfQuantisedClip ("1/8", "1920", R"(
        <Note tick="500" length="300" pitch="60" velocity="90"/>
        <Note tick="470" length="100" pitch="60" velocity="100"/>)");

    REQUIRE (clipTemplate.notes.size() == 2);
    CHECK (clipTemplate.notes[0].tick == 480);
    CHECK (clipTemplate.notes[0].velocity == 90);
    CHECK (clipTemplate.notes[1].tick == 480);
    CHECK (clipTemplate.notes[1].velocity == 100);
}

TEST_CASE ("A note at or past the loop length is silent in a quantised clip too", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfQuantisedClip ("1/8", "1000", R"(
        <Note tick="1000" length="10" pitch="61"/>
        <Note tick="1430" length="10" pitch="62"/>)");

    CHECK (clipTemplate.notes.empty());
}

TEST_CASE ("A template's events are in tick order, and in child order at one tick", "[render][SnapshotBuilder]")
{
    // Bank select, then the program change, at one tick: nothing is sorted
    // by kind or by controller number.
    const auto clipTemplate = templateOfEventClip ("3840", R"(
        <Event tick="480" kind="cc" a="7" b="100"/>
        <Event tick="0" kind="cc" a="32" b="2"/>
        <Event tick="0" kind="cc" a="0" b="1"/>
        <Event tick="0" kind="programChange" a="5"/>
        <Event tick="0" kind="cc" a="7" b="90"/>)");

    REQUIRE (clipTemplate.events.size() == 5);

    CHECK (clipTemplate.events[0].kind == ClipTemplate::EventKind::cc);
    CHECK (clipTemplate.events[0].a == 32);
    CHECK (clipTemplate.events[1].kind == ClipTemplate::EventKind::cc);
    CHECK (clipTemplate.events[1].a == 0);
    CHECK (clipTemplate.events[2].kind == ClipTemplate::EventKind::programChange);
    CHECK (clipTemplate.events[2].a == 5);
    CHECK (clipTemplate.events[3].kind == ClipTemplate::EventKind::cc);
    CHECK (clipTemplate.events[3].b == 90);

    CHECK (clipTemplate.events[4].tick == 480);
    CHECK (clipTemplate.events[4].b == 100);
}

TEST_CASE ("Every kind of event is in the template with its values", "[render][SnapshotBuilder]")
{
    const auto clipTemplate = templateOfEventClip ("3840", R"(
        <Event tick="0" kind="cc" a="74" b="90"/>
        <Event tick="10" kind="pitchBend" a="16383"/>
        <Event tick="20" kind="programChange" a="127"/>
        <Event tick="30" kind="channelPressure" a="55"/>
        <Event tick="40" kind="polyPressure" a="60" b="33"/>
        <Event tick="50" kind="sysex" data="7e7f0901"/>)");

    REQUIRE (clipTemplate.events.size() == 6);

    CHECK (clipTemplate.events[0].kind == ClipTemplate::EventKind::cc);
    CHECK (clipTemplate.events[0].a == 74);
    CHECK (clipTemplate.events[0].b == 90);

    CHECK (clipTemplate.events[1].kind == ClipTemplate::EventKind::pitchBend);
    CHECK (clipTemplate.events[1].a == 16383);
    CHECK (clipTemplate.events[1].b == 0);

    CHECK (clipTemplate.events[2].kind == ClipTemplate::EventKind::programChange);
    CHECK (clipTemplate.events[2].a == 127);

    CHECK (clipTemplate.events[3].kind == ClipTemplate::EventKind::channelPressure);
    CHECK (clipTemplate.events[3].a == 55);

    CHECK (clipTemplate.events[4].kind == ClipTemplate::EventKind::polyPressure);
    CHECK (clipTemplate.events[4].a == 60);
    CHECK (clipTemplate.events[4].b == 33);

    CHECK (clipTemplate.events[5].kind == ClipTemplate::EventKind::sysex);
    CHECK (clipTemplate.events[5].tick == 50);
}

TEST_CASE ("A sysex event's payload is in the template as bytes", "[render][SnapshotBuilder]")
{
    // Upper and lower case digits; a disabled event's payload is left out,
    // and so is one past the loop. The second event stands first in time.
    const auto clipTemplate = templateOfEventClip ("960", R"(
        <Event tick="100" kind="sysex" data="7E7f09"/>
        <Event tick="0" kind="sysex" data="00ff"/>
        <Event tick="0" kind="sysex" data="1234" enabled="0"/>
        <Event tick="960" kind="sysex" data="5678"/>
        <Event tick="200" kind="sysex"/>
        <Event tick="300" kind="cc" a="1" b="2"/>)");

    CHECK (clipTemplate.sysexBytes == std::vector<std::uint8_t> { 0x7e, 0x7f, 0x09, 0x00, 0xff });
    REQUIRE (clipTemplate.events.size() == 4);

    CHECK (clipTemplate.events[0].tick == 0);
    CHECK (clipTemplate.events[0].dataStart == 3);
    CHECK (clipTemplate.events[0].dataLength == 2);

    CHECK (clipTemplate.events[1].tick == 100);
    CHECK (clipTemplate.events[1].dataStart == 0);
    CHECK (clipTemplate.events[1].dataLength == 3);

    // A sysex event without a payload.
    CHECK (clipTemplate.events[2].kind == ClipTemplate::EventKind::sysex);
    CHECK (clipTemplate.events[2].dataLength == 0);

    CHECK (clipTemplate.events[3].kind == ClipTemplate::EventKind::cc);
    CHECK (clipTemplate.events[3].dataStart == 0);
    CHECK (clipTemplate.events[3].dataLength == 0);
}

TEST_CASE ("A step clip's template has the loop length that follows from its grid", "[render][SnapshotBuilder]")
{
    // Eight columns of 240 ticks, the fourth skipped, reset after the sixth:
    // five audible columns, 1200 ticks (the example of docs/SCHEMA.md).
    const auto project = projectWith (
        oneTempo,
        R"(<Clip id="11111111111141118111111111111111" kind="step">
             <Grid stepLength="240" stepCount="8" resetAt="6" skipMask="00010000"/>
           </Clip>)",
        "");

    const auto clipTemplate = SnapshotBuilder::templateOf (model::ProjectModel (project).getClipPool().getClips().front());

    CHECK (clipTemplate.loopLength == 1200);
}

TEST_CASE ("The Timeline's tempos become segments in tick order, in whole microseconds", "[render][SnapshotBuilder]")
{
    const auto project = projectWith (
        R"(<TempoEvent tick="7680" bpm="118"/>
           <TempoEvent tick="0" bpm="120"/>
           <TimeSigEvent tick="0" numerator="4" denominator="4"/>
           <TempoEvent tick="3840" bpm="60"/>)",
        "",
        "");

    const auto segments = SnapshotBuilder::tempoSegmentsOf (model::ProjectModel (project).getTimeline());

    REQUIRE (segments.size() == 3);
    CHECK (segments[0].startTick == 0);
    CHECK (segments[0].microsecondsPerQuarter == 500000);
    CHECK (segments[1].startTick == 3840);
    CHECK (segments[1].microsecondsPerQuarter == 1000000);
    // 60 000 000 / 118 = 508 474.58, played as 508 475 [D13].
    CHECK (segments[2].startTick == 7680);
    CHECK (segments[2].microsecondsPerQuarter == 508475);
}

TEST_CASE ("A Snapshot's TempoMap plays the Timeline's tempos", "[render][SnapshotBuilder]")
{
    const auto project = projectWith (
        R"(<TempoEvent tick="960" bpm="60"/>
           <TempoEvent tick="0" bpm="120"/>
           <TimeSigEvent tick="0" numerator="4" denominator="4"/>)",
        "",
        "");

    const auto snapshot = SnapshotBuilder::build (model::ProjectModel (project));

    // A quarter note at 120 bpm is half a second, the next at 60 bpm a whole one.
    CHECK (snapshot.tempoMap.nanosecondsAt (960) == 500000000);
    CHECK (snapshot.tempoMap.nanosecondsAt (1920) == 1500000000);
}

TEST_CASE ("A document without tracks and clips gives a Snapshot without plans and templates", "[render][SnapshotBuilder]")
{
    const auto snapshot = SnapshotBuilder::build (model::ProjectModel (projectWith (oneTempo, "", "")));

    CHECK (snapshot.clipTemplates.empty());
    CHECK (snapshot.trackPlans.empty());
}

namespace
{

// Five clips: A and B are placed on tracks, C (cc events only) on an
// AutomationLane, D is in a slot only, and nothing references E. Their loop
// lengths tell them apart: 1000, 2000, 3000, 4000, 5000.
const char* const fiveClips = R"(
    <Clip id="aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa" name="A" kind="event" loopLength="1000">
      <Events><Note tick="0" length="100" pitch="60"/></Events>
    </Clip>
    <Clip id="eeeeeeeeeeee4eee8eeeeeeeeeeeeeee" name="E" kind="event" loopLength="5000"><Events/></Clip>
    <Clip id="bbbbbbbbbbbb4bbb8bbbbbbbbbbbbbbb" name="B" kind="event" loopLength="2000"><Events/></Clip>
    <Clip id="cccccccccccc4ccc8ccccccccccccccc" name="C" kind="event" loopLength="3000">
      <Events><Event tick="0" kind="cc" a="74" b="1"/></Events>
    </Clip>
    <Clip id="dddddddddddd4ddd8ddddddddddddddd" name="D" kind="event" loopLength="4000"><Events/></Clip>)";

// Three tracks. The first holds three placements, stored out of time order:
// B at 8000, A at 0 and A again at 3000 with an offset. The second has a
// lane with C and a slot with D, and no placement of its own. The third is
// empty.
const char* const threeTracks = R"(
    <Track id="10000000000040008000000000000001" name="One" channel="3">
      <Placements>
        <Placement id="20000000000040008000000000000001" clipId="bbbbbbbbbbbb4bbb8bbbbbbbbbbbbbbb" start="8000" length="500"/>
        <Placement id="20000000000040008000000000000002" clipId="aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa" start="0" length="3000"/>
        <Placement id="20000000000040008000000000000003" clipId="aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa" start="3000" length="2500" offset="250"/>
      </Placements>
      <AutomationLanes/><Knobs/><Slots/>
    </Track>
    <Track id="10000000000040008000000000000002" name="Two" channel="16">
      <Placements/>
      <AutomationLanes>
        <AutomationLane id="30000000000040008000000000000001">
          <Placement id="20000000000040008000000000000004" clipId="cccccccccccc4ccc8ccccccccccccccc" start="0" length="3000"/>
        </AutomationLane>
      </AutomationLanes>
      <Knobs/>
      <Slots><Slot scene="0" clipId="dddddddddddd4ddd8ddddddddddddddd"/></Slots>
    </Track>
    <Track id="10000000000040008000000000000003" name="Three">
      <Placements/><AutomationLanes/><Knobs/><Slots/>
    </Track>)";

} // namespace

TEST_CASE ("A Snapshot has a template for every referenced clip, in the order of the pool", "[render][SnapshotBuilder]")
{
    const auto snapshot = SnapshotBuilder::build (model::ProjectModel (projectWith (oneTempo, fiveClips, threeTracks)));

    // A and B through a track's placements, C through a lane's, D through a
    // slot; E, which nothing references, has none.
    REQUIRE (snapshot.clipTemplates.size() == 4);
    CHECK (snapshot.clipTemplates[0].loopLength == 1000);
    CHECK (snapshot.clipTemplates[1].loopLength == 2000);
    CHECK (snapshot.clipTemplates[2].loopLength == 3000);
    CHECK (snapshot.clipTemplates[3].loopLength == 4000);

    // A clip placed twice has one template.
    CHECK (snapshot.clipTemplates[0].notes.size() == 1);
    CHECK (snapshot.clipTemplates[2].events.size() == 1);
}

TEST_CASE ("A Snapshot has a plan for every track, in track order, with the track's channel", "[render][SnapshotBuilder]")
{
    const auto snapshot = SnapshotBuilder::build (model::ProjectModel (projectWith (oneTempo, fiveClips, threeTracks)));

    REQUIRE (snapshot.trackPlans.size() == 3);
    CHECK (snapshot.trackPlans[0].channel == 3);
    CHECK (snapshot.trackPlans[1].channel == 16);
    // The default of the schema.
    CHECK (snapshot.trackPlans[2].channel == 1);

    // A lane's placements and a slot are not placements of the track itself.
    CHECK (snapshot.trackPlans[0].placements.size() == 3);
    CHECK (snapshot.trackPlans[1].placements.empty());
    CHECK (snapshot.trackPlans[2].placements.empty());
}

TEST_CASE ("A plan's placements are in the order of their starts and name their clip's template", "[render][SnapshotBuilder]")
{
    const auto snapshot = SnapshotBuilder::build (model::ProjectModel (projectWith (oneTempo, fiveClips, threeTracks)));

    REQUIRE (snapshot.trackPlans.size() == 3);
    const auto& placements = snapshot.trackPlans[0].placements;
    REQUIRE (placements.size() == 3);

    CHECK (placements[0].start == 0);
    CHECK (placements[0].length == 3000);
    CHECK (placements[0].offset == 0);
    CHECK (placements[0].templateIndex == 0);

    CHECK (placements[1].start == 3000);
    CHECK (placements[1].length == 2500);
    CHECK (placements[1].offset == 250);
    CHECK (placements[1].templateIndex == 0);

    // B is the second template: E stands before it in the pool and has none.
    CHECK (placements[2].start == 8000);
    CHECK (placements[2].length == 500);
    CHECK (placements[2].offset == 0);
    CHECK (placements[2].templateIndex == 1);
}

TEST_CASE ("Without hardware the tracks of one Player share a destination, and a track without a Player has its own", "[render][SnapshotBuilder]")
{
    const juce::String players = R"(
        <Player id="aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa" name="One"/>
        <Player id="bbbbbbbbbbbb4bbb8bbbbbbbbbbbbbbb" name="Two"/>
        <Player id="cccccccccccc4ccc8ccccccccccccccc" name="Unused"/>)";

    const auto track = [] (const juce::String& id, const juce::String& playerId, int channel) {
        return R"(<Track id=")" + id + R"(" playerId=")" + playerId + R"(" channel=")" + juce::String (channel)
             + R"("><Placements/><AutomationLanes/><Knobs/><Slots/></Track>)";
    };

    const auto project = juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" ppq="960"><Routing>)" + players + R"(<PadMap/></Routing><Timeline>)" + oneTempo
        + "</Timeline><ClipPool/><Tracks>"
        + track ("11111111111141118111111111111111", "", 1)
        + track ("22222222222242228222222222222222", "bbbbbbbbbbbb4bbb8bbbbbbbbbbbbbbb", 1)
        + track ("33333333333343338333333333333333", "aaaaaaaaaaaa4aaa8aaaaaaaaaaaaaaa", 1)
        + track ("44444444444444448444444444444444", "", 1)
        // The channel plays no role: the tracker keeps the channels of a
        // destination apart itself.
        + track ("55555555555545558555555555555555", "bbbbbbbbbbbb4bbb8bbbbbbbbbbbbbbb", 2)
        + "</Tracks><Session/><UiState/></Project>");

    REQUIRE (project.isValid());
    REQUIRE (model::Validator::validate (project).empty());

    const auto destinations = SnapshotBuilder::destinationsWithoutHardware (model::ProjectModel (project));

    // Numbered in the order in which the tracks come to them; a Player no
    // track uses has none.
    CHECK (destinations.destinationOfTrack == std::vector<std::size_t> { 0, 1, 2, 3, 1 });
    CHECK (destinations.count == 4);
}

TEST_CASE ("A document without tracks has no destinations", "[render][SnapshotBuilder]")
{
    const auto destinations = SnapshotBuilder::destinationsWithoutHardware (model::ProjectModel (projectWith (oneTempo, "", "")));

    CHECK (destinations.destinationOfTrack.empty());
    CHECK (destinations.count == 0);
}
