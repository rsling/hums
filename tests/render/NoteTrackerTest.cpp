// NoteTracker: the note-offs of the notes that sound, per destination, and
// the same-pitch rule (docs/RENDERING.md -> Same-pitch overlap).

#include <catch2/catch_test_macros.hpp>

#include "render/NoteTracker.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using render::NoteTracker;

namespace
{
// An off on destination 0, channel 1, from track 0.
NoteTracker::NoteOff offAt (std::int64_t tick, int pitch)
{
    NoteTracker::NoteOff off;
    off.tick = tick;
    off.pitch = pitch;
    off.offVelocity = 64;
    return off;
}

// Every off due by the tick, in the order the tracker gives them.
std::vector<NoteTracker::NoteOff> takeAllDueBy (NoteTracker& tracker, std::int64_t tick)
{
    std::vector<NoteTracker::NoteOff> offs;

    while (const std::optional<NoteTracker::NoteOff> off = tracker.takeOffDueBy (tick))
        offs.push_back (*off);

    return offs;
}

std::vector<NoteTracker::NoteOff> takeAllAtStop (NoteTracker& tracker, std::int64_t stopTick)
{
    std::vector<NoteTracker::NoteOff> offs;

    while (const std::optional<NoteTracker::NoteOff> off = tracker.takeOffAtStop (stopTick))
        offs.push_back (*off);

    return offs;
}
} // namespace

TEST_CASE ("A tracker in which nothing sounds has no off to give", "[render][notetracker]")
{
    NoteTracker tracker (1);

    CHECK (tracker.soundingCount() == 0);
    CHECK_FALSE (tracker.takeOffDueBy (1000000).has_value());
    CHECK_FALSE (tracker.takeOffAtStop (0).has_value());
}

TEST_CASE ("A note's off is held until its tick and given once", "[render][notetracker]")
{
    NoteTracker tracker (1);

    NoteTracker::NoteOff off;
    off.tick = 480;
    off.destination = 0;
    off.channel = 10;
    off.pitch = 36;
    off.offVelocity = 23;
    off.track = 3;

    CHECK_FALSE (tracker.noteOn (0, off).has_value());
    CHECK (tracker.soundingCount() == 1);

    CHECK_FALSE (tracker.takeOffDueBy (479).has_value());
    CHECK (tracker.soundingCount() == 1);

    const std::optional<NoteTracker::NoteOff> due = tracker.takeOffDueBy (480);
    REQUIRE (due.has_value());
    CHECK (due->tick == 480);
    CHECK (due->destination == 0);
    CHECK (due->channel == 10);
    CHECK (due->pitch == 36);
    CHECK (due->offVelocity == 23);
    CHECK (due->track == 3);

    CHECK (tracker.soundingCount() == 0);
    CHECK_FALSE (tracker.takeOffDueBy (480).has_value());
}

TEST_CASE ("An off asked for after its tick keeps its tick", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (480, 60));

    const std::optional<NoteTracker::NoteOff> due = tracker.takeOffDueBy (960);
    REQUIRE (due.has_value());
    CHECK (due->tick == 480);
}

TEST_CASE ("Offs come in the order of their ticks, not of their note-ons", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (900, 60));
    tracker.noteOn (10, offAt (300, 62));
    tracker.noteOn (20, offAt (600, 64));

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 1000);
    REQUIRE (offs.size() == 3);
    CHECK (offs[0].pitch == 62);
    CHECK (offs[0].tick == 300);
    CHECK (offs[1].pitch == 64);
    CHECK (offs[1].tick == 600);
    CHECK (offs[2].pitch == 60);
    CHECK (offs[2].tick == 900);
}

TEST_CASE ("Offs of one tick come in the order in which their notes began", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (480, 67));
    tracker.noteOn (0, offAt (480, 60));
    tracker.noteOn (100, offAt (240, 72));
    tracker.noteOn (200, offAt (480, 64));

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 480);
    REQUIRE (offs.size() == 4);
    CHECK (offs[0].pitch == 72);
    CHECK (offs[1].pitch == 67);
    CHECK (offs[2].pitch == 60);
    CHECK (offs[3].pitch == 64);
}

TEST_CASE ("Only the offs due by the tick are given", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (240, 60));
    tracker.noteOn (0, offAt (480, 62));

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 240);
    REQUIRE (offs.size() == 1);
    CHECK (offs[0].pitch == 60);
    CHECK (tracker.soundingCount() == 1);
}

TEST_CASE ("A note-on of a pitch that still sounds gets the earlier note's off first", "[render][notetracker]")
{
    NoteTracker tracker (1);

    NoteTracker::NoteOff first = offAt (960, 60);
    first.offVelocity = 11;
    first.track = 0;
    tracker.noteOn (0, first);

    NoteTracker::NoteOff second = offAt (1200, 60);
    second.offVelocity = 22;
    second.track = 1;

    // The earlier note is ended where the new one begins, with its own off
    // velocity and for its own track.
    const std::optional<NoteTracker::NoteOff> before = tracker.noteOn (480, second);
    REQUIRE (before.has_value());
    CHECK (before->tick == 480);
    CHECK (before->pitch == 60);
    CHECK (before->offVelocity == 11);
    CHECK (before->track == 0);

    CHECK (tracker.soundingCount() == 1);

    // The off the earlier note would have had at 960 is never given.
    CHECK_FALSE (tracker.takeOffDueBy (960).has_value());

    const std::optional<NoteTracker::NoteOff> due = tracker.takeOffDueBy (1200);
    REQUIRE (due.has_value());
    CHECK (due->tick == 1200);
    CHECK (due->offVelocity == 22);
    CHECK (due->track == 1);
}

TEST_CASE ("A shorter note of the same pitch inside a longer one ends the sound at its own end", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (1920, 60));

    const std::optional<NoteTracker::NoteOff> before = tracker.noteOn (240, offAt (480, 60));
    REQUIRE (before.has_value());
    CHECK (before->tick == 240);

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 1920);
    REQUIRE (offs.size() == 1);
    CHECK (offs[0].tick == 480);
}

TEST_CASE ("Two notes of one pitch at one tick are on, off, on", "[render][notetracker]")
{
    NoteTracker tracker (1);

    NoteTracker::NoteOff first = offAt (480, 60);
    first.offVelocity = 11;
    CHECK_FALSE (tracker.noteOn (0, first).has_value());

    NoteTracker::NoteOff second = offAt (240, 60);
    second.offVelocity = 22;

    const std::optional<NoteTracker::NoteOff> before = tracker.noteOn (0, second);
    REQUIRE (before.has_value());
    CHECK (before->tick == 0);
    CHECK (before->offVelocity == 11);

    // The second note's length counts.
    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 480);
    REQUIRE (offs.size() == 1);
    CHECK (offs[0].tick == 240);
    CHECK (offs[0].offVelocity == 22);
}

TEST_CASE ("An off that was due before the new note-on keeps its tick", "[render][notetracker]")
{
    // The offs due are meant to be taken before a tick's note-ons. If they
    // were not, the earlier note's off is still not moved to a later tick.
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (240, 60));

    const std::optional<NoteTracker::NoteOff> before = tracker.noteOn (480, offAt (960, 60));
    REQUIRE (before.has_value());
    CHECK (before->tick == 240);
    CHECK (tracker.soundingCount() == 1);
}

TEST_CASE ("Another pitch, another channel and another destination are other notes", "[render][notetracker]")
{
    NoteTracker tracker (2);
    tracker.noteOn (0, offAt (960, 60));

    NoteTracker::NoteOff otherPitch = offAt (960, 61);
    CHECK_FALSE (tracker.noteOn (100, otherPitch).has_value());

    NoteTracker::NoteOff otherChannel = offAt (960, 60);
    otherChannel.channel = 2;
    CHECK_FALSE (tracker.noteOn (100, otherChannel).has_value());

    NoteTracker::NoteOff otherDestination = offAt (960, 60);
    otherDestination.destination = 1;
    CHECK_FALSE (tracker.noteOn (100, otherDestination).has_value());

    CHECK (tracker.soundingCount() == 4);
}

TEST_CASE ("Two tracks on one destination and channel share their pitches", "[render][notetracker]")
{
    NoteTracker tracker (1);

    NoteTracker::NoteOff ofTrack0 = offAt (960, 60);
    ofTrack0.track = 0;
    tracker.noteOn (0, ofTrack0);

    NoteTracker::NoteOff ofTrack1 = offAt (720, 60);
    ofTrack1.track = 1;

    const std::optional<NoteTracker::NoteOff> before = tracker.noteOn (480, ofTrack1);
    REQUIRE (before.has_value());
    CHECK (before->tick == 480);
    CHECK (before->track == 0);

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 960);
    REQUIRE (offs.size() == 1);
    CHECK (offs[0].tick == 720);
    CHECK (offs[0].track == 1);
}

TEST_CASE ("A stop gives every off that is held, later ones at the stop", "[render][notetracker]")
{
    NoteTracker tracker (2);
    tracker.noteOn (0, offAt (2000, 60));
    tracker.noteOn (0, offAt (1500, 62));

    NoteTracker::NoteOff onOtherDestination = offAt (3000, 60);
    onOtherDestination.destination = 1;
    tracker.noteOn (0, onOtherDestination);

    // One that was due before the stop and has not been taken yet.
    tracker.noteOn (0, offAt (500, 64));

    const std::vector<NoteTracker::NoteOff> offs = takeAllAtStop (tracker, 1000);
    REQUIRE (offs.size() == 4);

    CHECK (offs[0].pitch == 64);
    CHECK (offs[0].tick == 500);

    CHECK (offs[1].pitch == 62);
    CHECK (offs[1].tick == 1000);

    CHECK (offs[2].pitch == 60);
    CHECK (offs[2].destination == 0);
    CHECK (offs[2].tick == 1000);

    CHECK (offs[3].pitch == 60);
    CHECK (offs[3].destination == 1);
    CHECK (offs[3].tick == 1000);

    CHECK (tracker.soundingCount() == 0);
    CHECK_FALSE (tracker.takeOffDueBy (5000).has_value());
}

TEST_CASE ("After a stop a pitch that sounded begins anew", "[render][notetracker]")
{
    NoteTracker tracker (1);
    tracker.noteOn (0, offAt (2000, 60));
    takeAllAtStop (tracker, 1000);

    CHECK_FALSE (tracker.noteOn (0, offAt (480, 60)).has_value());
    CHECK (tracker.soundingCount() == 1);
}

TEST_CASE ("Every pitch of every channel of every destination can sound at once", "[render][notetracker]")
{
    // The most there can be: the tracker has room for exactly this, and a
    // second note-on of each pitch takes the place of the first.
    const std::size_t destinationCount = 2;
    const std::size_t most = destinationCount * 16 * 128;

    NoteTracker tracker (destinationCount);

    for (int round = 0; round < 2; ++round)
    {
        std::size_t offsOfEarlierNotes = 0;

        for (std::size_t destination = 0; destination < destinationCount; ++destination)
        {
            for (int channel = 1; channel <= 16; ++channel)
            {
                for (int pitch = 0; pitch < 128; ++pitch)
                {
                    NoteTracker::NoteOff off;
                    off.tick = 1000 + pitch;
                    off.destination = destination;
                    off.channel = channel;
                    off.pitch = pitch;

                    if (tracker.noteOn (round, off).has_value())
                        ++offsOfEarlierNotes;
                }
            }
        }

        CHECK (offsOfEarlierNotes == (round == 0 ? 0 : most));
        CHECK (tracker.soundingCount() == most);
    }

    const std::vector<NoteTracker::NoteOff> offs = takeAllDueBy (tracker, 2000);
    REQUIRE (offs.size() == most);

    for (std::size_t position = 1; position < offs.size(); ++position)
        REQUIRE (offs[position - 1].tick <= offs[position].tick);
}
