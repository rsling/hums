// render::Kernel: a Snapshot over a window of ticks (docs/RENDERING.md -> Per
// track, Note windows). The Snapshots here are made by hand; the rules
// themselves have their golden files (tests/render/golden/).

#include <catch2/catch_test_macros.hpp>

#include "render/Kernel.h"
#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/Snapshot.h"
#include "render/snapshot/TrackPlan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

using render::ClipTemplate;
using render::Kernel;
using render::Snapshot;
using render::TrackPlan;

namespace
{

const std::int64_t largestTick = std::numeric_limits<std::int64_t>::max();

ClipTemplate::Note noteAt (std::int64_t tick, std::int64_t length, int pitch)
{
    ClipTemplate::Note note;
    note.tick = tick;
    note.length = length;
    note.pitch = pitch;
    note.velocity = 100;
    note.offVelocity = 64;
    return note;
}

ClipTemplate::Event ccAt (std::int64_t tick, int controller, int value)
{
    ClipTemplate::Event event;
    event.tick = tick;
    event.kind = ClipTemplate::EventKind::cc;
    event.a = controller;
    event.b = value;
    return event;
}

ClipTemplate templateWith (std::int64_t loopLength, std::vector<ClipTemplate::Note> notes, std::vector<ClipTemplate::Event> events = {})
{
    ClipTemplate clipTemplate;
    clipTemplate.loopLength = loopLength;
    clipTemplate.notes = std::move (notes);
    clipTemplate.events = std::move (events);
    return clipTemplate;
}

TrackPlan planWith (int channel, std::vector<TrackPlan::Placement> placements)
{
    TrackPlan plan;
    plan.channel = channel;
    plan.placements = std::move (placements);
    return plan;
}

Snapshot snapshotWith (std::vector<ClipTemplate> clipTemplates, std::vector<TrackPlan> trackPlans)
{
    return Snapshot { render::TempoMap ({ { 0, 500000 } }), std::move (clipTemplates), std::move (trackPlans) };
}

// One track on channel 1 with one placement of one clip.
Snapshot onePlacementOf (ClipTemplate clipTemplate, std::int64_t start, std::int64_t length, std::int64_t offset)
{
    return snapshotWith ({ std::move (clipTemplate) }, { planWith (1, { { start, length, offset, 0 } }) });
}

// Everything from `start` up to `end`, asked for with a buffer of the given
// size as often as it takes.
std::vector<Kernel::Event> expandAll (const Snapshot& snapshot, const std::vector<bool>& audibleTracks, std::int64_t start, std::int64_t end, std::size_t bufferSize = 16)
{
    std::vector<Kernel::Event> events;
    std::vector<Kernel::Event> buffer (bufferSize);

    for (Kernel::Position position { start, 0 }; position.tick < end;)
    {
        const auto progress = Kernel::expand (snapshot, audibleTracks, position, end, buffer);

        // A call that gives nothing must have come to the end.
        REQUIRE ((progress.eventCount > 0 || progress.next.tick == end));

        events.insert (events.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t> (progress.eventCount));
        position = progress.next;
    }

    return events;
}

std::vector<Kernel::Event> expandAll (const Snapshot& snapshot, std::int64_t start, std::int64_t end)
{
    return expandAll (snapshot, std::vector<bool> (snapshot.trackPlans.size(), true), start, end);
}

bool areTheSame (const Kernel::Event& one, const Kernel::Event& other)
{
    return one.tick == other.tick && one.track == other.track && one.channel == other.channel && one.kind == other.kind
        && one.a == other.a && one.b == other.b && one.offTick == other.offTick && one.offVelocity == other.offVelocity
        && one.templateIndex == other.templateIndex && one.dataStart == other.dataStart && one.dataLength == other.dataLength;
}

bool areTheSame (const std::vector<Kernel::Event>& one, const std::vector<Kernel::Event>& other)
{
    if (one.size() != other.size())
        return false;

    for (std::size_t index = 0; index < one.size(); ++index)
        if (! areTheSame (one[index], other[index]))
            return false;

    return true;
}

ClipTemplate::Event programChangeAt (std::int64_t tick, int program)
{
    ClipTemplate::Event event;
    event.tick = tick;
    event.kind = ClipTemplate::EventKind::programChange;
    event.a = program;
    return event;
}

// A template of events only, in tick order, with the chase tables that
// SnapshotBuilder makes from them: the cc events by controller, in the
// order of the controller numbers, and the program changes.
ClipTemplate chasedTemplateWith (std::int64_t loopLength, std::vector<ClipTemplate::Event> events)
{
    ClipTemplate clipTemplate = templateWith (loopLength, {}, std::move (events));

    for (int controller = 0; controller < 128; ++controller)
    {
        ClipTemplate::ControllerChase controllerChase;
        controllerChase.controller = controller;

        for (const auto& event : clipTemplate.events)
            if (event.kind == ClipTemplate::EventKind::cc && event.a == controller)
                controllerChase.points.push_back ({ event.tick, event.b });

        if (! controllerChase.points.empty())
            clipTemplate.controllerChases.push_back (controllerChase);
    }

    for (const auto& event : clipTemplate.events)
        if (event.kind == ClipTemplate::EventKind::programChange)
            clipTemplate.programChanges.push_back ({ event.tick, event.a });

    return clipTemplate;
}

// What track 0 has last sent of a controller before a tick.
int chasedController (const Snapshot& snapshot, int controller, std::int64_t tick)
{
    return Kernel::chase (snapshot, 0, tick).controllerValues[static_cast<std::size_t> (controller)];
}

const int none = Kernel::ChasedValues::none;

// Two tracks, several placements, notes and events that share ticks: enough
// for the ways of asking to differ if they could.
Snapshot busySnapshot()
{
    return snapshotWith (
        { templateWith (480, { noteAt (0, 100, 60), noteAt (0, 600, 64), noteAt (240, 50, 67) }, { ccAt (0, 1, 10), ccAt (0, 2, 20), ccAt (240, 1, 30) }),
          templateWith (300, { noteAt (100, 500, 72) }, { ccAt (299, 7, 99) }) },
        { planWith (1, { { 0, 1200, 0, 0 }, { 1200, 700, 240, 0 } }),
          planWith (2, { { 240, 900, 50, 1 }, { 1500, 480, 0, 0 } }) });
}

} // namespace

TEST_CASE ("A note comes out as a note-on at its tick in the placement, with the tick of its off", "[render][kernel]")
{
    ClipTemplate::Note note = noteAt (240, 480, 60);
    note.velocity = 90;
    note.offVelocity = 30;

    const auto events = expandAll (onePlacementOf (templateWith (1920, { note }), 3840, 1920, 0), 0, 10000);

    REQUIRE (events.size() == 1);
    CHECK (events[0].tick == 4080);
    CHECK (events[0].track == 0);
    CHECK (events[0].channel == 1);
    CHECK (events[0].kind == Kernel::EventKind::noteOn);
    CHECK (events[0].a == 60);
    CHECK (events[0].b == 90);
    CHECK (events[0].offTick == 4560);
    CHECK (events[0].offVelocity == 30);
}

TEST_CASE ("An event comes out with its kind and values, a sysex event with the place of its payload", "[render][kernel]")
{
    ClipTemplate::Event sysex;
    sysex.tick = 10;
    sysex.kind = ClipTemplate::EventKind::sysex;
    sysex.dataStart = 3;
    sysex.dataLength = 5;

    ClipTemplate::Event bend;
    bend.tick = 20;
    bend.kind = ClipTemplate::EventKind::pitchBend;
    bend.a = 8192;

    // The second template of the two is the placement's.
    const auto snapshot = snapshotWith ({ templateWith (960, {}), templateWith (960, {}, { sysex, bend }) },
                                        { planWith (7, { { 0, 960, 0, 1 } }) });
    const auto events = expandAll (snapshot, 0, 960);

    REQUIRE (events.size() == 2);
    CHECK (events[0].tick == 10);
    CHECK (events[0].channel == 7);
    CHECK (events[0].kind == Kernel::EventKind::sysex);
    CHECK (events[0].templateIndex == 1);
    CHECK (events[0].dataStart == 3);
    CHECK (events[0].dataLength == 5);

    CHECK (events[1].tick == 20);
    CHECK (events[1].kind == Kernel::EventKind::pitchBend);
    CHECK (events[1].a == 8192);
    CHECK (events[1].b == 0);
}

TEST_CASE ("The clip loops inside its placement, and the last pass ends with the placement", "[render][kernel]")
{
    // Two and a half passes of a loop of 960.
    const auto events = expandAll (onePlacementOf (templateWith (960, { noteAt (0, 100, 60), noteAt (480, 100, 62) }), 1000, 2400, 0), 0, 10000);

    REQUIRE (events.size() == 5);
    CHECK (events[0].tick == 1000);
    CHECK (events[1].tick == 1480);
    CHECK (events[2].tick == 1960);
    CHECK (events[3].tick == 2440);
    CHECK (events[4].tick == 2920);
    // The third pass would come to tick 480 of the clip at 3400, which is
    // where the placement ends: no longer played.
}

TEST_CASE ("A placement with an offset begins that far into the loop", "[render][kernel]")
{
    const auto events = expandAll (onePlacementOf (templateWith (960, { noteAt (0, 100, 60), noteAt (480, 100, 62) }), 0, 960, 300), 0, 10000);

    // Tick 480 of the clip comes 180 ticks in; tick 0 only when the loop has
    // come round, 660 ticks in.
    REQUIRE (events.size() == 2);
    CHECK (events[0].tick == 180);
    CHECK (events[0].a == 62);
    CHECK (events[1].tick == 660);
    CHECK (events[1].a == 60);
}

TEST_CASE ("A note that began before the offset is not brought in at the placement's start", "[render][kernel]")
{
    // The note sounds from 0 to 900 of the loop; the placement begins at 300.
    const auto events = expandAll (onePlacementOf (templateWith (960, { noteAt (0, 900, 60) }), 0, 600, 300), 0, 10000);

    CHECK (events.empty());
}

TEST_CASE ("A note plays its full length across the loop point", "[render][kernel]")
{
    const auto events = expandAll (onePlacementOf (templateWith (960, { noteAt (900, 200, 60) }), 0, 2880, 0), 0, 10000);

    REQUIRE (events.size() == 3);
    CHECK (events[0].tick == 900);
    CHECK (events[0].offTick == 1100);
    CHECK (events[1].tick == 1860);
    CHECK (events[1].offTick == 2060);
    // The placement ends at 2880 and cuts the last one.
    CHECK (events[2].tick == 2820);
    CHECK (events[2].offTick == 2880);
}

TEST_CASE ("The end of its placement cuts a note, also when the next placement follows at once", "[render][kernel]")
{
    const auto snapshot = snapshotWith ({ templateWith (960, { noteAt (400, 400, 60) }) },
                                        { planWith (1, { { 0, 500, 0, 0 }, { 500, 960, 0, 0 } }) });
    const auto events = expandAll (snapshot, 0, 10000);

    REQUIRE (events.size() == 2);
    CHECK (events[0].tick == 400);
    CHECK (events[0].offTick == 500);
    CHECK (events[1].tick == 900);
    CHECK (events[1].offTick == 1300);
}

TEST_CASE ("A placement that goes on where the one before ends does not cut a note", "[render][kernel]")
{
    // The first placement ends at tick 500 of the loop, and the second
    // begins there: what a split at 500 leaves.
    const auto snapshot = snapshotWith ({ templateWith (960, { noteAt (400, 400, 60) }) },
                                        { planWith (1, { { 0, 500, 0, 0 }, { 500, 960, 500, 0 } }) });
    const auto events = expandAll (snapshot, 0, 10000);

    // The note is not begun again at the seam, and the loop is not either.
    REQUIRE (events.size() == 2);
    CHECK (events[0].tick == 400);
    CHECK (events[0].offTick == 800);
    // The second pass of the loop begins at 960; the merged placement ends
    // at 1460 and cuts its note.
    CHECK (events[1].tick == 1360);
    CHECK (events[1].offTick == 1460);
}

TEST_CASE ("A placement goes on at the tick of the loop to which an offset and several passes have brought the one before", "[render][kernel]")
{
    // The first placement begins 300 into a loop of 960 and is 1200 long: it
    // ends at tick (300 + 1200) mod 960 = 540.
    const auto snapshot = snapshotWith ({ templateWith (960, { noteAt (500, 200, 60) }) },
                                        { planWith (1, { { 0, 1200, 300, 0 }, { 1200, 600, 540, 0 } }) });
    const auto events = expandAll (snapshot, 0, 10000);

    REQUIRE (events.size() == 2);
    CHECK (events[0].tick == 200);
    CHECK (events[0].offTick == 400);
    // Across the seam at 1200.
    CHECK (events[1].tick == 1160);
    CHECK (events[1].offTick == 1360);
}

TEST_CASE ("A note sounds on through as many placements as continue each other, and is cut where the last ends", "[render][kernel]")
{
    const auto clipTemplate = templateWith (1920, { noteAt (0, 1500, 60) });

    SECTION ("the note ends inside the third")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 500, 500, 500, 0 }, { 1000, 700, 1000, 0 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 1500);
    }

    SECTION ("the third ends before the note does")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 500, 500, 500, 0 }, { 1000, 300, 1000, 0 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 1300);
    }

    SECTION ("the third does not continue the second")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 500, 500, 500, 0 }, { 1000, 700, 1100, 0 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 1000);
    }
}

TEST_CASE ("Placements are one only with the same clip, no gap and the phase continued", "[render][kernel]")
{
    // Two templates with the same content: two clips.
    const auto clipTemplate = templateWith (960, { noteAt (400, 400, 60) });

    SECTION ("another clip")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate, clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 500, 300, 500, 1 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 500);
    }

    SECTION ("a gap of one tick")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 501, 300, 500, 0 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 500);
    }

    SECTION ("an offset one tick off")
    {
        const auto events = expandAll (snapshotWith ({ clipTemplate }, { planWith (1, { { 0, 500, 0, 0 }, { 500, 300, 501, 0 } }) }), 0, 10000);

        REQUIRE (events.size() == 1);
        CHECK (events[0].offTick == 500);
    }
}

TEST_CASE ("At one tick a track's events come before its notes, each in the template's order, track after track", "[render][kernel]")
{
    const auto snapshot = snapshotWith (
        { templateWith (960, { noteAt (0, 100, 60), noteAt (0, 100, 59) }, { ccAt (0, 2, 20), ccAt (0, 1, 10) }) },
        { planWith (1, { { 0, 960, 0, 0 } }), planWith (2, { { 0, 960, 0, 0 } }) });
    const auto events = expandAll (snapshot, 0, 960);

    REQUIRE (events.size() == 8);

    for (std::size_t track = 0; track < 2; ++track)
    {
        const std::size_t first = track * 4;

        CHECK (events[first].track == track);
        CHECK (events[first].kind == Kernel::EventKind::cc);
        CHECK (events[first].a == 2);
        CHECK (events[first + 1].kind == Kernel::EventKind::cc);
        CHECK (events[first + 1].a == 1);
        CHECK (events[first + 2].kind == Kernel::EventKind::noteOn);
        CHECK (events[first + 2].a == 60);
        CHECK (events[first + 3].kind == Kernel::EventKind::noteOn);
        CHECK (events[first + 3].a == 59);
        CHECK (events[first + 3].track == track);
    }
}

TEST_CASE ("A track that is not heard gives no events", "[render][kernel]")
{
    const auto snapshot = snapshotWith ({ templateWith (960, { noteAt (0, 100, 60) }) },
                                        { planWith (1, { { 0, 960, 0, 0 } }), planWith (2, { { 0, 960, 0, 0 } }) });

    const auto events = expandAll (snapshot, { false, true }, 0, 960);

    REQUIRE (events.size() == 1);
    CHECK (events[0].track == 1);

    CHECK (expandAll (snapshot, { false, false }, 0, 960).empty());
}

TEST_CASE ("The end of a window is not played, and its start is", "[render][kernel]")
{
    const auto snapshot = onePlacementOf (templateWith (960, { noteAt (100, 10, 60) }), 0, 960, 0);

    CHECK (expandAll (snapshot, 0, 100).empty());
    CHECK (expandAll (snapshot, 100, 101).size() == 1);
    CHECK (expandAll (snapshot, 101, 960).empty());
    CHECK (expandAll (snapshot, 100, 100).empty());
}

TEST_CASE ("A placement whose clip plays nothing, and the gap between two placements, are passed over", "[render][kernel]")
{
    const auto snapshot = snapshotWith ({ templateWith (960, {}), templateWith (960, { noteAt (0, 100, 60) }) },
                                        { planWith (1, { { 0, 960, 0, 0 }, { 5000, 960, 0, 1 } }) });
    const auto events = expandAll (snapshot, 0, 10000);

    REQUIRE (events.size() == 1);
    CHECK (events[0].tick == 5000);
}

TEST_CASE ("A snapshot without tracks, and one without placements, give nothing", "[render][kernel]")
{
    CHECK (expandAll (snapshotWith ({}, {}), 0, 10000).empty());
    CHECK (expandAll (snapshotWith ({}, { planWith (1, {}) }), 0, 10000).empty());
}

TEST_CASE ("A buffer of any size gives the same events, also where many share a tick", "[render][kernel]")
{
    const auto snapshot = busySnapshot();
    const std::vector<bool> allTracks (2, true);

    const auto inOnePiece = expandAll (snapshot, allTracks, 0, 3000, 1000);
    REQUIRE (inOnePiece.size() > 20);

    for (const std::size_t bufferSize : { 1u, 2u, 3u, 5u, 7u })
    {
        INFO ("buffer size " << bufferSize);
        CHECK (areTheSame (expandAll (snapshot, allTracks, 0, 3000, bufferSize), inOnePiece));
    }
}

TEST_CASE ("A stretch of ticks gives the same events in one window as in many", "[render][kernel]")
{
    const auto snapshot = busySnapshot();
    const auto inOneWindow = expandAll (snapshot, 0, 3000);

    for (const std::int64_t windowLength : { 1, 7, 240, 480, 1001 })
    {
        INFO ("window length " << windowLength);
        std::vector<Kernel::Event> inManyWindows;

        for (std::int64_t start = 0; start < 3000; start += windowLength)
        {
            const auto events = expandAll (snapshot, start, std::min<std::int64_t> (start + windowLength, 3000));
            inManyWindows.insert (inManyWindows.end(), events.begin(), events.end());
        }

        CHECK (areTheSame (inManyWindows, inOneWindow));
    }
}

TEST_CASE ("The largest ticks a document can hold are played without overflow", "[render][kernel]")
{
    // A loop nearly as long as time itself, an offset nearly as large, and a
    // placement that ends at the largest tick.
    const std::int64_t loopLength = largestTick - 10;
    const std::int64_t placementStart = 1000;
    const std::int64_t placementLength = largestTick - placementStart;
    const std::int64_t offset = loopLength - 5;

    const auto snapshot = onePlacementOf (templateWith (loopLength, { noteAt (0, largestTick - 20, 60), noteAt (loopLength - 3, 100, 62) }),
                                          placementStart,
                                          placementLength,
                                          offset);

    const auto atTheStart = expandAll (snapshot, 0, 5000);

    // Tick loopLength - 3 of the clip comes 2 ticks in, tick 0 after 5.
    REQUIRE (atTheStart.size() == 2);
    CHECK (atTheStart[0].tick == 1002);
    CHECK (atTheStart[0].a == 62);
    CHECK (atTheStart[0].offTick == 1102);
    CHECK (atTheStart[1].tick == 1005);
    CHECK (atTheStart[1].a == 60);
    // Longer than the placement has left: cut at its end.
    CHECK (atTheStart[1].offTick == largestTick);

    // The second pass begins loopLength ticks after the first tick 0, which
    // is past the placement's end: nothing more up to the last tick.
    CHECK (expandAll (snapshot, 5000, largestTick).empty());
}

TEST_CASE ("The chase gives the last value before the tick in the pass of the loop in hand", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (chasedTemplateWith (1920, { ccAt (0, 74, 10), ccAt (480, 74, 50), ccAt (1440, 74, 90) }), 0, 1920, 0);

    CHECK (chasedController (snapshot, 74, 960) == 50);
    CHECK (chasedController (snapshot, 74, 481) == 50);
    CHECK (chasedController (snapshot, 74, 1441) == 90);

    // What stands at the tick itself is not before it.
    CHECK (chasedController (snapshot, 74, 480) == 10);
    CHECK (chasedController (snapshot, 74, 0) == none);

    // A controller the clip never sends.
    CHECK (chasedController (snapshot, 7, 960) == none);
    CHECK (Kernel::chase (snapshot, 0, 960).program == none);
}

TEST_CASE ("The chase is counted in the placement: from its start, not from tick 0", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (chasedTemplateWith (960, { ccAt (100, 74, 10) }), 5000, 960, 0);

    CHECK (chasedController (snapshot, 74, 5000) == none);
    CHECK (chasedController (snapshot, 74, 5100) == none);
    CHECK (chasedController (snapshot, 74, 5101) == 10);
}

TEST_CASE ("Where the pass in hand has sent nothing yet, the chase gives the last value of the pass before", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (chasedTemplateWith (960, { ccAt (480, 74, 70), ccAt (720, 74, 30) }), 0, 3840, 0);

    // 80 ticks into the third pass.
    CHECK (chasedController (snapshot, 74, 2000) == 30);
    // On the loop point itself.
    CHECK (chasedController (snapshot, 74, 960) == 30);
    // In the first pass there is no pass before.
    CHECK (chasedController (snapshot, 74, 400) == none);
    // And once the pass in hand has sent one, that one counts.
    CHECK (chasedController (snapshot, 74, 2401) == 70);
}

TEST_CASE ("What stands before a placement's offset is not chased in its first pass", "[render][kernel][chase]")
{
    // The placement begins 300 ticks into the loop: tick t is tick 300 + t
    // of the clip.
    const auto snapshot = onePlacementOf (chasedTemplateWith (960, { ccAt (100, 74, 11), ccAt (500, 74, 55) }), 0, 1920, 300);

    // Tick 400 of the clip: the value of tick 100 lies before it in the
    // loop, and was never played.
    CHECK (chasedController (snapshot, 74, 100) == none);
    CHECK (chasedController (snapshot, 74, 200) == none);
    CHECK (chasedController (snapshot, 74, 201) == 55);

    // The second pass begins at 660 and plays the loop from its tick 0.
    CHECK (chasedController (snapshot, 74, 660) == 55);
    CHECK (chasedController (snapshot, 74, 760) == 55);
    CHECK (chasedController (snapshot, 74, 761) == 11);
}

TEST_CASE ("A first pass that played none of a table gives the second pass nothing to fall back on", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (chasedTemplateWith (960, { ccAt (100, 74, 11) }), 0, 1920, 300);

    CHECK (chasedController (snapshot, 74, 600) == none);
    // The second pass begins at 660; its tick 100 comes at 760.
    CHECK (chasedController (snapshot, 74, 660) == none);
    CHECK (chasedController (snapshot, 74, 760) == none);
    CHECK (chasedController (snapshot, 74, 761) == 11);
}

TEST_CASE ("What the end of a placement cuts off is not chased, however far behind it the tick lies", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (chasedTemplateWith (960, { ccAt (0, 74, 1), ccAt (800, 74, 99) }), 0, 500, 0);

    CHECK (chasedController (snapshot, 74, 500) == 1);
    CHECK (chasedController (snapshot, 74, 900) == 1);
    CHECK (chasedController (snapshot, 74, 50000) == 1);
}

TEST_CASE ("A placement that has no value yet leaves the controller to the placement before it", "[render][kernel][chase]")
{
    // "Intro" from 0 to 960, a gap, "Verse" from 1920.
    const auto snapshot = snapshotWith (
        { chasedTemplateWith (960, { ccAt (0, 7, 90), ccAt (0, 74, 40), ccAt (480, 74, 80) }),
          chasedTemplateWith (960, { ccAt (0, 9, 5), ccAt (720, 7, 60) }) },
        { planWith (1, { { 0, 960, 0, 0 }, { 1920, 1920, 0, 1 } }) });

    // In the gap, and on the tick at which the second placement begins.
    CHECK (chasedController (snapshot, 7, 1500) == 90);
    CHECK (chasedController (snapshot, 74, 1920) == 80);
    CHECK (chasedController (snapshot, 9, 1920) == none);

    // 80 ticks into "Verse": its own cc 9, and of "Intro" what "Verse" has
    // not sent yet or never sends.
    const auto early = Kernel::chase (snapshot, 0, 2000);
    CHECK (early.controllerValues[9] == 5);
    CHECK (early.controllerValues[7] == 90);
    CHECK (early.controllerValues[74] == 80);

    // Once "Verse" has sent its cc 7, that is the last one.
    const auto late = Kernel::chase (snapshot, 0, 2641);
    CHECK (late.controllerValues[7] == 60);
    CHECK (late.controllerValues[74] == 80);
}

TEST_CASE ("Program changes are chased like a controller, and of two values at one tick the later one counts", "[render][kernel][chase]")
{
    const auto snapshot = onePlacementOf (
        chasedTemplateWith (960, { programChangeAt (0, 3), programChangeAt (0, 8), ccAt (0, 7, 1), ccAt (0, 7, 2), programChangeAt (500, 12) }),
        0,
        1920,
        0);

    CHECK (Kernel::chase (snapshot, 0, 0).program == none);
    CHECK (Kernel::chase (snapshot, 0, 1).program == 8);
    CHECK (chasedController (snapshot, 7, 1) == 2);
    CHECK (Kernel::chase (snapshot, 0, 501).program == 12);
    CHECK (Kernel::chase (snapshot, 0, 961).program == 8);
}

TEST_CASE ("Each track is chased by itself, and a track without placements has no values", "[render][kernel][chase]")
{
    const auto snapshot = snapshotWith ({ chasedTemplateWith (960, { ccAt (0, 7, 100) }), chasedTemplateWith (960, { ccAt (0, 7, 50) }) },
                                        { planWith (1, { { 0, 960, 0, 0 } }), planWith (2, { { 0, 960, 0, 1 } }), planWith (3, {}) });

    CHECK (Kernel::chase (snapshot, 0, 500).controllerValues[7] == 100);
    CHECK (Kernel::chase (snapshot, 1, 500).controllerValues[7] == 50);

    const auto ofEmptyTrack = Kernel::chase (snapshot, 2, 500);
    CHECK (ofEmptyTrack.program == none);
    CHECK (std::all_of (ofEmptyTrack.controllerValues.begin(), ofEmptyTrack.controllerValues.end(), [] (int value) { return value == none; }));
}

TEST_CASE ("The largest ticks a document can hold are chased without overflow", "[render][kernel][chase]")
{
    // As in the test of expand: a loop nearly as long as time itself, an
    // offset nearly as large, and a placement that ends at the largest tick.
    const std::int64_t loopLength = largestTick - 10;
    const std::int64_t placementStart = 1000;
    const std::int64_t offset = loopLength - 5;

    const auto snapshot = onePlacementOf (chasedTemplateWith (loopLength, { ccAt (0, 74, 10), ccAt (loopLength - 3, 74, 20) }),
                                          placementStart,
                                          largestTick - placementStart,
                                          offset);

    // Tick loopLength - 3 of the clip comes 2 ticks in, tick 0 after 5.
    CHECK (chasedController (snapshot, 74, 1002) == none);
    CHECK (chasedController (snapshot, 74, 1003) == 20);
    CHECK (chasedController (snapshot, 74, 1005) == 20);
    CHECK (chasedController (snapshot, 74, 1006) == 10);
    CHECK (chasedController (snapshot, 74, largestTick) == 10);
}
