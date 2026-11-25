// render::Renderer: the Kernel's events put through the NoteTracker
// (docs/RENDERING.md -> Pipeline, Equal-tick order, Same-pitch overlap). The
// Snapshots here are made by hand; the rules themselves have their golden
// files (tests/render/golden/).

#include <catch2/catch_test_macros.hpp>

#include "render/Kernel.h"
#include "render/NoteTracker.h"
#include "render/Renderer.h"
#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/Snapshot.h"
#include "render/snapshot/TrackPlan.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using render::ClipTemplate;
using render::Kernel;
using render::NoteTracker;
using render::Renderer;
using render::Snapshot;
using render::TrackPlan;

namespace
{

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

ClipTemplate::Event programChangeAt (std::int64_t tick, int program)
{
    ClipTemplate::Event event;
    event.tick = tick;
    event.kind = ClipTemplate::EventKind::programChange;
    event.a = program;
    return event;
}

// A template with the chase tables that SnapshotBuilder makes from its
// events: the cc events by controller, in the order of the controller
// numbers, and the program changes.
ClipTemplate withChaseTables (ClipTemplate clipTemplate)
{
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

// One placement of each template on a track of its own, all on channel 1,
// from tick 0 for the given length.
Snapshot oneTrackPerTemplate (std::vector<ClipTemplate> clipTemplates, std::int64_t placementLength)
{
    std::vector<TrackPlan> trackPlans;

    for (std::size_t index = 0; index < clipTemplates.size(); ++index)
    {
        TrackPlan plan;
        plan.channel = 1;
        plan.placements.push_back ({ 0, placementLength, 0, index });
        trackPlans.push_back (plan);
    }

    return Snapshot { render::TempoMap ({ { 0, 500000 } }), std::move (clipTemplates), std::move (trackPlans) };
}

// What the tests look at of an event, as a text: "tick track kind a b".
std::string textOf (const Renderer::Event& event)
{
    const char* kind = "other";

    if (event.kind == Renderer::EventKind::noteOff)
        kind = "off";
    else if (event.kind == Renderer::EventKind::noteOn)
        kind = "on";
    else if (event.kind == Renderer::EventKind::cc)
        kind = "cc";
    else if (event.kind == Renderer::EventKind::programChange)
        kind = "program";

    return std::to_string (event.tick) + " " + std::to_string (event.track) + " " + kind + " " + std::to_string (event.a) + " " + std::to_string (event.b);
}

// A stretch of playback: the tracker lives from one window to the next.
struct Playback
{
    Playback (const Snapshot& snapshotToPlay, std::vector<std::size_t> destinations, std::size_t destinationCount, std::size_t bufferSizeToUse = 64)
        : snapshot (snapshotToPlay), destinationOfTrack (std::move (destinations)), tracker (destinationCount), buffer (bufferSizeToUse)
    {
    }

    // The window [start, end), asked for as often as the buffer makes
    // necessary.
    std::vector<std::string> render (std::int64_t start, std::int64_t end)
    {
        std::vector<std::string> texts;
        const std::vector<bool> allTracks (snapshot.trackPlans.size(), true);
        Kernel::Position position { start, 0 };

        for (bool isFinished = false; ! isFinished;)
        {
            const auto progress = Renderer::render (snapshot, allTracks, destinationOfTrack, tracker, position, end, buffer);

            // A call that writes nothing must have finished.
            REQUIRE ((progress.eventCount > 0 || progress.isFinished));

            for (std::size_t index = 0; index < progress.eventCount; ++index)
                texts.push_back (textOf (buffer[index]));

            position = progress.next;
            isFinished = progress.isFinished;
        }

        return texts;
    }

    std::vector<std::string> stop (std::int64_t stopTick)
    {
        std::vector<std::string> texts;

        do
        {
            const std::size_t offCount = Renderer::stop (tracker, stopTick, buffer);

            for (std::size_t index = 0; index < offCount; ++index)
                texts.push_back (textOf (buffer[index]));
        } while (tracker.soundingCount() > 0);

        return texts;
    }

    // Playback leaves one tick for another, every track heard: asked for as
    // often as the buffer makes necessary.
    std::vector<std::string> seek (std::int64_t leftTick, std::int64_t targetTick)
    {
        return seek (leftTick, targetTick, std::vector<bool> (snapshot.trackPlans.size(), true));
    }

    std::vector<std::string> seek (std::int64_t leftTick, std::int64_t targetTick, const std::vector<bool>& audibleTracks)
    {
        std::vector<std::string> texts;
        Renderer::ChasePosition position;

        for (bool isFinished = false; ! isFinished;)
        {
            const auto progress = Renderer::seek (snapshot, audibleTracks, destinationOfTrack, tracker, leftTick, targetTick, position, buffer);

            // A call that writes nothing must have finished.
            REQUIRE ((progress.eventCount > 0 || progress.isFinished));

            for (std::size_t index = 0; index < progress.eventCount; ++index)
                texts.push_back (textOf (buffer[index]));

            position = progress.next;
            isFinished = progress.isFinished;
        }

        return texts;
    }

    const Snapshot& snapshot;
    std::vector<std::size_t> destinationOfTrack;
    NoteTracker tracker;
    std::vector<Renderer::Event> buffer;
};

using Texts = std::vector<std::string>;

} // namespace

TEST_CASE ("A note goes out as a note-on and, when it is due, its note-off", "[render][renderer]")
{
    ClipTemplate::Note note = noteAt (100, 200, 60);
    note.velocity = 90;
    note.offVelocity = 30;

    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { note }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 960) == Texts { "100 0 on 60 90", "300 0 off 60 30" });
    CHECK (playback.tracker.soundingCount() == 0);
}

TEST_CASE ("An event carries its track's destination and channel", "[render][renderer]")
{
    auto snapshot = oneTrackPerTemplate ({ templateWith (960, {}), templateWith (960, { noteAt (0, 10, 60) }, { ccAt (0, 7, 100) }) }, 960);
    snapshot.trackPlans[1].channel = 5;

    NoteTracker tracker (4);
    std::vector<Renderer::Event> buffer (8);
    const std::vector<std::size_t> destinationOfTrack { 0, 3 };

    const auto progress = Renderer::render (snapshot, { true, true }, destinationOfTrack, tracker, { 0, 0 }, 960, buffer);

    REQUIRE (progress.isFinished);
    REQUIRE (progress.eventCount == 3);

    for (std::size_t index = 0; index < 3; ++index)
    {
        CHECK (buffer[index].track == 1);
        CHECK (buffer[index].destination == 3);
        CHECK (buffer[index].channel == 5);
    }

    CHECK (buffer[0].kind == Renderer::EventKind::cc);
    CHECK (buffer[1].kind == Renderer::EventKind::noteOn);
    CHECK (buffer[2].kind == Renderer::EventKind::noteOff);
}

TEST_CASE ("At one tick the note-offs that are due come first, then the events, then the note-ons", "[render][renderer]")
{
    // The first note ends at 480, where a controller and the next note stand.
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 480, 60), noteAt (480, 100, 62) }, { ccAt (480, 7, 100) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 960) == Texts { "0 0 on 60 100", "480 0 off 60 64", "480 0 cc 7 100", "480 0 on 62 100", "580 0 off 62 64" });
}

TEST_CASE ("A note-off between two events goes out at its own tick, in the order of the ticks", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 300, 60), noteAt (100, 50, 62) }, { ccAt (200, 7, 1), ccAt (400, 7, 2) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 960)
           == Texts { "0 0 on 60 100", "100 0 on 62 100", "150 0 off 62 64", "200 0 cc 7 1", "300 0 off 60 64", "400 0 cc 7 2" });
}

TEST_CASE ("A note-off on the end of a window belongs to the next window", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 480, 60) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 480) == Texts { "0 0 on 60 100" });
    CHECK (playback.tracker.soundingCount() == 1);
    CHECK (playback.render (480, 960) == Texts { "480 0 off 60 64" });
}

TEST_CASE ("A note that sounds across several windows gets its off in the window it falls in", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (3840, { noteAt (10, 2000, 60) }) }, 3840);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 960) == Texts { "10 0 on 60 100" });
    CHECK (playback.render (960, 1920).empty());
    CHECK (playback.render (1920, 2880) == Texts { "2010 0 off 60 64" });
}

TEST_CASE ("Stopping ends every note that sounds: at the stop, or at its own tick if that is earlier", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 480, 60), noteAt (100, 800, 62) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 600) == Texts { "0 0 on 60 100", "100 0 on 62 100", "480 0 off 60 64" });
    CHECK (playback.stop (600) == Texts { "600 0 off 62 64" });
    CHECK (playback.tracker.soundingCount() == 0);
    CHECK (playback.stop (600).empty());
}

TEST_CASE ("A range that ends where a note-off falls gets that off from the stop", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 960, 60) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 960) == Texts { "0 0 on 60 100" });
    CHECK (playback.stop (960) == Texts { "960 0 off 60 64" });
}

TEST_CASE ("Two tracks on one destination share their pitches: the earlier note's off comes before the new note-on", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 600, 60) }), templateWith (960, { noteAt (200, 100, 60) }) }, 960);
    Playback playback (snapshot, { 0, 0 }, 1);

    // The first note's own off at 600 is not sent a second time.
    CHECK (playback.render (0, 960) == Texts { "0 0 on 60 100", "200 0 off 60 64", "200 1 on 60 100", "300 1 off 60 64" });
}

TEST_CASE ("Two tracks on two destinations share nothing", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 600, 60) }), templateWith (960, { noteAt (200, 100, 60) }) }, 960);
    Playback playback (snapshot, { 0, 1 }, 2);

    CHECK (playback.render (0, 960) == Texts { "0 0 on 60 100", "200 1 on 60 100", "300 1 off 60 64", "600 0 off 60 64" });
}

TEST_CASE ("A buffer of any size from two events up gives the same events", "[render][renderer]")
{
    // Chords, repeated pitches and controllers on shared ticks, over more
    // events than the Renderer asks the Kernel for at a time.
    const auto snapshot = oneTrackPerTemplate (
        { templateWith (240, { noteAt (0, 300, 60), noteAt (0, 100, 64), noteAt (0, 240, 67), noteAt (120, 500, 60) }, { ccAt (0, 1, 1), ccAt (0, 2, 2), ccAt (239, 3, 3) }),
          templateWith (100, { noteAt (0, 100, 60), noteAt (50, 75, 72) }, { ccAt (50, 9, 9) }) },
        4800);

    Playback inOnePiece (snapshot, { 0, 0 }, 1, 4096);
    auto expected = inOnePiece.render (0, 4800);
    const auto expectedAtStop = inOnePiece.stop (4800);
    REQUIRE (expected.size() > 300);

    for (const std::size_t bufferSize : { 2u, 3u, 5u, 64u, 65u })
    {
        INFO ("buffer size " << bufferSize);
        Playback playback (snapshot, { 0, 0 }, 1, bufferSize);

        CHECK (playback.render (0, 4800) == expected);
        CHECK (playback.stop (4800) == expectedAtStop);
    }
}

TEST_CASE ("A stretch of ticks gives the same events in one window as in many", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate (
        { templateWith (240, { noteAt (0, 300, 60), noteAt (0, 100, 64), noteAt (120, 500, 60) }, { ccAt (0, 1, 1), ccAt (239, 3, 3) }),
          templateWith (100, { noteAt (0, 100, 60), noteAt (50, 75, 72) }, { ccAt (50, 9, 9) }) },
        2400);

    Playback inOneWindow (snapshot, { 0, 0 }, 1);
    const auto expected = inOneWindow.render (0, 2400);

    for (const std::int64_t windowLength : { 1, 7, 100, 240, 1001 })
    {
        INFO ("window length " << windowLength);
        Playback playback (snapshot, { 0, 0 }, 1);
        Texts inManyWindows;

        for (std::int64_t start = 0; start < 2400; start += windowLength)
        {
            const auto texts = playback.render (start, start + windowLength < 2400 ? start + windowLength : 2400);
            inManyWindows.insert (inManyWindows.end(), texts.begin(), texts.end());
        }

        CHECK (inManyWindows == expected);
    }
}

TEST_CASE ("An empty window gives nothing and is finished", "[render][renderer]")
{
    const auto snapshot = oneTrackPerTemplate ({ templateWith (960, { noteAt (0, 480, 60) }) }, 960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 0).empty());
    CHECK (playback.render (500, 500).empty());
}

TEST_CASE ("A seek sends what a track has last sent before the target: bank select, the program change, then the other controllers by number", "[render][renderer][seek]")
{
    // In the template the order is another one.
    const auto snapshot = oneTrackPerTemplate (
        { withChaseTables (templateWith (960, {}, { ccAt (0, 74, 10), ccAt (0, 7, 100), programChangeAt (0, 5), ccAt (0, 32, 2), ccAt (0, 1, 33), ccAt (0, 0, 1), ccAt (480, 74, 50), ccAt (600, 10, 64) })) },
        960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.seek (600, 600) == Texts { "600 0 cc 0 1", "600 0 cc 32 2", "600 0 program 5 0", "600 0 cc 1 33", "600 0 cc 7 100", "600 0 cc 74 50" });

    // What stands at the target itself is played from there, not chased.
    CHECK (playback.render (600, 960) == Texts { "600 0 cc 10 64" });
}

TEST_CASE ("A seek that finds a program change and no bank select, or the other way round, sends what there is", "[render][renderer][seek]")
{
    const auto snapshot = oneTrackPerTemplate ({ withChaseTables (templateWith (960, {}, { programChangeAt (0, 9), ccAt (0, 7, 100) })),
                                                 withChaseTables (templateWith (960, {}, { ccAt (0, 32, 4), ccAt (0, 127, 1) })) },
                                               960);
    Playback playback (snapshot, { 0, 0 }, 1);

    CHECK (playback.seek (0, 500) == Texts { "500 0 program 9 0", "500 0 cc 7 100", "500 1 cc 32 4", "500 1 cc 127 1" });
}

TEST_CASE ("A chased value carries its track's destination and channel and the tick of the target", "[render][renderer][seek]")
{
    auto snapshot = oneTrackPerTemplate ({ templateWith (960, {}), withChaseTables (templateWith (960, {}, { ccAt (0, 7, 100) })) }, 960);
    snapshot.trackPlans[1].channel = 5;

    NoteTracker tracker (4);
    std::vector<Renderer::Event> buffer (8);
    const std::vector<std::size_t> destinationOfTrack { 0, 3 };

    const auto progress = Renderer::seek (snapshot, { true, true }, destinationOfTrack, tracker, 100, 700, {}, buffer);

    REQUIRE (progress.isFinished);
    REQUIRE (progress.eventCount == 1);
    CHECK (buffer[0].tick == 700);
    CHECK (buffer[0].track == 1);
    CHECK (buffer[0].destination == 3);
    CHECK (buffer[0].channel == 5);
    CHECK (buffer[0].kind == Renderer::EventKind::cc);
    CHECK (buffer[0].a == 7);
    CHECK (buffer[0].b == 100);
}

TEST_CASE ("A seek first ends every note that sounds, at the tick it leaves, and begins none again", "[render][renderer][seek]")
{
    // Note 60 sounds from 0 to 800, note 62 from 100 to 300.
    const auto snapshot = oneTrackPerTemplate (
        { withChaseTables (templateWith (960, { noteAt (0, 800, 60), noteAt (100, 200, 62) }, { ccAt (0, 7, 100) })) },
        960);
    Playback playback (snapshot, { 0 }, 1);

    CHECK (playback.render (0, 600) == Texts { "0 0 cc 7 100", "0 0 on 60 100", "100 0 on 62 100", "300 0 off 62 64" });

    // Back to 200, where both notes would be sounding.
    CHECK (playback.seek (600, 200) == Texts { "600 0 off 60 64", "200 0 cc 7 100" });
    CHECK (playback.tracker.soundingCount() == 0);

    // The off of note 62 does not come a second time: it was not begun.
    CHECK (playback.render (200, 400).empty());
}

TEST_CASE ("The wrap of the transport loop is a seek from the loop's end to its start", "[render][renderer][seek]")
{
    const auto snapshot = oneTrackPerTemplate (
        { withChaseTables (templateWith (1920, { noteAt (1440, 960, 60) }, { ccAt (0, 7, 100), ccAt (480, 74, 30) })) },
        1920);
    Playback playback (snapshot, { 0 }, 1);

    // The loop is 960 to 1920; an off on the loop's end is the wrap's.
    CHECK (playback.render (0, 1920) == Texts { "0 0 cc 7 100", "480 0 cc 74 30", "1440 0 on 60 100" });
    CHECK (playback.seek (1920, 960) == Texts { "1920 0 off 60 64", "960 0 cc 7 100", "960 0 cc 74 30" });
    CHECK (playback.render (960, 1920) == Texts { "1440 0 on 60 100" });
}

TEST_CASE ("A track that is not heard is not chased; the others come in track order", "[render][renderer][seek]")
{
    const auto snapshot = oneTrackPerTemplate ({ withChaseTables (templateWith (960, {}, { ccAt (0, 7, 1) })),
                                                 withChaseTables (templateWith (960, {}, { ccAt (0, 7, 2) })),
                                                 withChaseTables (templateWith (960, {}, { ccAt (0, 7, 3) })) },
                                               960);
    Playback playback (snapshot, { 0, 0, 0 }, 1);

    CHECK (playback.seek (0, 500) == Texts { "500 0 cc 7 1", "500 1 cc 7 2", "500 2 cc 7 3" });
    CHECK (playback.seek (0, 500, { true, false, true }) == Texts { "500 0 cc 7 1", "500 2 cc 7 3" });
    CHECK (playback.seek (0, 500, { false, false, false }).empty());
}

TEST_CASE ("A seek with nothing sounding and nothing to chase gives nothing and is finished", "[render][renderer][seek]")
{
    const auto snapshot = oneTrackPerTemplate ({ withChaseTables (templateWith (960, { noteAt (0, 100, 60) }, { ccAt (500, 7, 1) })) }, 960);

    NoteTracker tracker (1);
    std::vector<Renderer::Event> buffer (4);
    const std::vector<std::size_t> destinationOfTrack { 0 };

    const auto progress = Renderer::seek (snapshot, { true }, destinationOfTrack, tracker, 300, 300, {}, buffer);

    CHECK (progress.eventCount == 0);
    CHECK (progress.isFinished);
}

TEST_CASE ("A seek gives the same events through a buffer of any size", "[render][renderer][seek]")
{
    // Many notes that sound, and many values on several tracks, one of them
    // without any.
    std::vector<ClipTemplate::Note> chord;
    std::vector<ClipTemplate::Event> controllers { programChangeAt (0, 7) };

    for (int index = 0; index < 20; ++index)
    {
        chord.push_back (noteAt (0, 900, 40 + index));
        controllers.push_back (ccAt (0, index * 6, index));
    }

    const auto snapshot = oneTrackPerTemplate ({ withChaseTables (templateWith (960, chord, controllers)),
                                                 templateWith (960, {}),
                                                 withChaseTables (templateWith (960, {}, controllers)) },
                                               960);

    Playback inOnePiece (snapshot, { 0, 1, 1 }, 2, 4096);
    inOnePiece.render (0, 500);
    const auto expected = inOnePiece.seek (500, 300);
    REQUIRE (expected.size() == 20 + 21 + 21);

    for (const std::size_t bufferSize : { 1u, 2u, 3u, 7u, 21u, 64u })
    {
        INFO ("buffer size " << bufferSize);
        Playback playback (snapshot, { 0, 1, 1 }, 2, 4096);
        playback.render (0, 500);
        playback.buffer.resize (bufferSize);

        CHECK (playback.seek (500, 300) == expected);
    }
}
