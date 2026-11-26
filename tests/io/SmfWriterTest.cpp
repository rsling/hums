// io::SmfWriter: an SmfFile as the bytes of a Standard MIDI File, every event
// where and in the order in which the SmfFile has it
// (docs/RENDERING.md -> SMF bytes). The expected bytes are written out here
// by hand.

#include <catch2/catch_test_macros.hpp>

#include "io/SmfReader.h"
#include "io/SmfWriter.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{

using Bytes = std::vector<std::uint8_t>;

io::SmfEvent channelMessage (std::int64_t tick, std::uint8_t status, Bytes data)
{
    io::SmfEvent event;
    event.tick = tick;
    event.status = status;
    event.bytes = std::move (data);
    return event;
}

io::SmfEvent metaEvent (std::int64_t tick, std::uint8_t type, Bytes payload)
{
    io::SmfEvent event;
    event.tick = tick;
    event.status = io::SmfEvent::metaStatus;
    event.metaType = type;
    event.bytes = std::move (payload);
    return event;
}

io::SmfEvent sysexEvent (std::int64_t tick, std::uint8_t status, Bytes payload)
{
    io::SmfEvent event;
    event.tick = tick;
    event.status = status;
    event.bytes = std::move (payload);
    return event;
}

io::SmfFile fileOfOneTrack (std::vector<io::SmfEvent> events, std::int64_t endTick)
{
    io::SmfFile file;
    file.tracks.push_back ({ std::move (events), endTick });
    return file;
}

Bytes written (const io::SmfFile& file)
{
    const auto result = io::SmfWriter::write (file);
    INFO (result.message.toStdString());
    REQUIRE (result.wasWritten);
    CHECK (result.message.isEmpty());

    return result.bytes;
}

// The bytes of the one track of a file after the chunk's type and length,
// which begin at byte 22.
Bytes writtenTrackBody (std::vector<io::SmfEvent> events, std::int64_t endTick)
{
    const auto bytes = written (fileOfOneTrack (std::move (events), endTick));
    REQUIRE (bytes.size() >= 22);

    return { bytes.begin() + 22, bytes.end() };
}

std::string refusalOf (const io::SmfFile& file)
{
    const auto result = io::SmfWriter::write (file);
    CHECK_FALSE (result.wasWritten);
    CHECK (result.bytes.empty());

    return result.message.toStdString();
}

// A file with every kind of event, events at one tick, running status begun,
// ended and begun again, and delta times of every length.
io::SmfFile fileWithEverything()
{
    io::SmfFile file;
    file.format = 1;
    file.division = 960;

    file.tracks.push_back ({ { metaEvent (0, 0x51, { 0x07, 0xa1, 0x20 }),
                               metaEvent (0, 0x58, { 0x04, 0x02, 0x18, 0x08 }),
                               metaEvent (3840, 0x06, { 0x41 }) },
                             7680 });

    file.tracks.push_back ({ { metaEvent (0, 0x03, { 0x42, 0x61, 0x73, 0x73 }),
                               channelMessage (0, 0xb0, { 0x00, 0x01 }),
                               channelMessage (0, 0xb0, { 0x20, 0x02 }),
                               channelMessage (0, 0xc0, { 0x05 }),
                               channelMessage (0, 0x90, { 0x3c, 0x64 }),
                               channelMessage (0, 0x80, { 0x3c, 0x40 }),
                               channelMessage (0, 0x90, { 0x3c, 0x50 }),
                               channelMessage (127, 0x90, { 0x3e, 0x00 }),
                               sysexEvent (255, 0xf0, { 0x7e, 0x7f, 0x09, 0x01, 0xf7 }),
                               sysexEvent (255, 0xf7, { 0xf8 }),
                               channelMessage (16638, 0x90, { 0x40, 0x64 }),
                               channelMessage (33022, 0xe0, { 0x00, 0x40 }),
                               channelMessage (2130174, 0xd0, { 0x7f }),
                               channelMessage (2130174, 0xa0, { 0x3c, 0x7f }),
                               channelMessage (270565629, 0x80, { 0x40, 0x00 }) },
                             270565629 });

    file.tracks.push_back ({ {}, 0 });

    return file;
}

} // namespace

TEST_CASE ("A file is written as a header and one chunk for each track", "[io][SmfWriter]")
{
    io::SmfFile file;
    file.format = 1;
    file.division = 480;
    file.tracks.push_back ({ { metaEvent (0, 0x51, { 0x07, 0xa1, 0x20 }) }, 480 });
    file.tracks.push_back ({ { channelMessage (0, 0x90, { 0x3c, 0x64 }), channelMessage (96, 0x80, { 0x3c, 0x40 }) }, 96 });

    const Bytes expected {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x01, 0xe0,
        'M', 'T', 'r', 'k', 0, 0, 0, 12,
        0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
        0x83, 0x60, 0xff, 0x2f, 0x00,
        'M', 'T', 'r', 'k', 0, 0, 0, 12,
        0x00, 0x90, 0x3c, 0x64,
        0x60, 0x80, 0x3c, 0x40,
        0x00, 0xff, 0x2f, 0x00
    };

    CHECK (written (file) == expected);
}

TEST_CASE ("The header says the type, the number of tracks and the division as they are", "[io][SmfWriter]")
{
    SECTION ("type 0")
    {
        io::SmfFile file;
        file.format = 0;
        file.division = 96;
        file.tracks.push_back ({ {}, 0 });

        CHECK (written (file) == Bytes { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 0x60, 'M', 'T', 'r', 'k', 0, 0, 0, 4, 0x00, 0xff, 0x2f, 0x00 });
    }

    SECTION ("type 2, SMPTE time, no track")
    {
        io::SmfFile file;
        file.format = 2;
        file.division = 0xe728;

        CHECK (written (file) == Bytes { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 2, 0, 0, 0xe7, 0x28 });
    }

    SECTION ("300 tracks")
    {
        io::SmfFile file;
        file.tracks.resize (300);

        const auto bytes = written (file);
        CHECK (bytes[10] == 0x01);
        CHECK (bytes[11] == 0x2c);
        CHECK (bytes.size() == 14 + 300 * 12);
    }
}

TEST_CASE ("Events at one tick are written in the order they have", "[io][SmfWriter]")
{
    SECTION ("a note-on before the note-off of its pitch")
    {
        CHECK (writtenTrackBody ({ channelMessage (5, 0x90, { 0x3c, 0x64 }), channelMessage (5, 0x80, { 0x3c, 0x40 }) }, 5)
               == Bytes { 0x05, 0x90, 0x3c, 0x64,
                          0x00, 0x80, 0x3c, 0x40,
                          0x00, 0xff, 0x2f, 0x00 });
    }

    SECTION ("a note-off before the note-on of its pitch")
    {
        CHECK (writtenTrackBody ({ channelMessage (5, 0x80, { 0x3c, 0x40 }), channelMessage (5, 0x90, { 0x3c, 0x64 }) }, 5)
               == Bytes { 0x05, 0x80, 0x3c, 0x40,
                          0x00, 0x90, 0x3c, 0x64,
                          0x00, 0xff, 0x2f, 0x00 });
    }

    SECTION ("bank select, program change and note, with a meta event among them")
    {
        CHECK (writtenTrackBody ({ channelMessage (0, 0xb0, { 0x00, 0x01 }),
                                   channelMessage (0, 0xb0, { 0x20, 0x02 }),
                                   metaEvent (0, 0x06, { 0x41 }),
                                   channelMessage (0, 0xc0, { 0x05 }),
                                   channelMessage (0, 0x90, { 0x3c, 0x64 }) },
                                 0)
               == Bytes { 0x00, 0xb0, 0x00, 0x01,
                          0x00, 0x20, 0x02,
                          0x00, 0xff, 0x06, 0x01, 0x41,
                          0x00, 0xc0, 0x05,
                          0x00, 0x90, 0x3c, 0x64,
                          0x00, 0xff, 0x2f, 0x00 });
    }
}

TEST_CASE ("A channel message of the status before it is written without its status byte", "[io][SmfWriter]")
{
    SECTION ("as long as the status stays the same")
    {
        CHECK (writtenTrackBody ({ channelMessage (0, 0x91, { 0x3c, 0x64 }),
                                   channelMessage (16, 0x91, { 0x3e, 0x50 }),
                                   channelMessage (32, 0x91, { 0x3c, 0x00 }),
                                   // another channel, another kind of message
                                   channelMessage (32, 0x92, { 0x3c, 0x64 }),
                                   channelMessage (32, 0x82, { 0x3c, 0x40 }),
                                   channelMessage (32, 0xc2, { 0x05 }),
                                   channelMessage (33, 0xc2, { 0x06 }) },
                                 33)
               == Bytes { 0x00, 0x91, 0x3c, 0x64,
                          0x10, 0x3e, 0x50,
                          0x10, 0x3c, 0x00,
                          0x00, 0x92, 0x3c, 0x64,
                          0x00, 0x82, 0x3c, 0x40,
                          0x00, 0xc2, 0x05,
                          0x01, 0x06,
                          0x00, 0xff, 0x2f, 0x00 });
    }

    SECTION ("and with it again after a meta, a SysEx and an escape event")
    {
        CHECK (writtenTrackBody ({ channelMessage (0, 0x90, { 0x3c, 0x64 }),
                                   metaEvent (0, 0x06, {}),
                                   channelMessage (0, 0x90, { 0x3e, 0x64 }),
                                   sysexEvent (0, 0xf0, { 0x7e, 0xf7 }),
                                   channelMessage (0, 0x90, { 0x40, 0x64 }),
                                   sysexEvent (0, 0xf7, { 0xf8 }),
                                   channelMessage (0, 0x90, { 0x41, 0x64 }) },
                                 0)
               == Bytes { 0x00, 0x90, 0x3c, 0x64,
                          0x00, 0xff, 0x06, 0x00,
                          0x00, 0x90, 0x3e, 0x64,
                          0x00, 0xf0, 0x02, 0x7e, 0xf7,
                          0x00, 0x90, 0x40, 0x64,
                          0x00, 0xf7, 0x01, 0xf8,
                          0x00, 0x90, 0x41, 0x64,
                          0x00, 0xff, 0x2f, 0x00 });
    }
}

TEST_CASE ("A delta time is written in as few bytes as it takes", "[io][SmfWriter]")
{
    const auto deltaBytes = [] (std::int64_t delta) {
        // An empty track that ends at `delta`: the delta time, then FF 2F 00.
        auto body = writtenTrackBody ({}, delta);
        body.resize (body.size() - 3);
        return body;
    };

    CHECK (deltaBytes (0) == Bytes { 0x00 });
    CHECK (deltaBytes (127) == Bytes { 0x7f });
    CHECK (deltaBytes (128) == Bytes { 0x81, 0x00 });
    CHECK (deltaBytes (480) == Bytes { 0x83, 0x60 });
    CHECK (deltaBytes (16383) == Bytes { 0xff, 0x7f });
    CHECK (deltaBytes (16384) == Bytes { 0x81, 0x80, 0x00 });
    CHECK (deltaBytes (2097151) == Bytes { 0xff, 0xff, 0x7f });
    CHECK (deltaBytes (2097152) == Bytes { 0x81, 0x80, 0x80, 0x00 });
    CHECK (deltaBytes (268435455) == Bytes { 0xff, 0xff, 0xff, 0x7f });
}

TEST_CASE ("A tick is counted from the start of the track, a delta time from the event before", "[io][SmfWriter]")
{
    // Ticks far beyond one delta time are reached in steps.
    CHECK (writtenTrackBody ({ channelMessage (268435455, 0xc0, { 0x01 }), channelMessage (536870910, 0xc0, { 0x02 }) }, 536870911)
           == Bytes { 0xff, 0xff, 0xff, 0x7f, 0xc0, 0x01,
                      0xff, 0xff, 0xff, 0x7f, 0x02,
                      0x01, 0xff, 0x2f, 0x00 });
}

TEST_CASE ("A payload has its length before it", "[io][SmfWriter]")
{
    Bytes payload (200, 0x55);

    auto expected = Bytes { 0x00, 0xff, 0x01, 0x81, 0x48 };
    expected.insert (expected.end(), payload.begin(), payload.end());
    expected.insert (expected.end(), { 0x00, 0xf0, 0x81, 0x48 });
    expected.insert (expected.end(), payload.begin(), payload.end());
    expected.insert (expected.end(), { 0x00, 0xff, 0x2f, 0x00 });

    CHECK (writtenTrackBody ({ metaEvent (0, 0x01, payload), sysexEvent (0, 0xf0, payload) }, 0) == expected);

    // And the chunk the length of its track: 205 bytes of the meta event, 204 of the SysEx, 4 of End of Track.
    // 413 is 0x019d.
    const auto bytes = written (fileOfOneTrack ({ metaEvent (0, 0x01, payload), sysexEvent (0, 0xf0, payload) }, 0));
    CHECK (Bytes { bytes.begin() + 18, bytes.begin() + 22 } == Bytes { 0x00, 0x00, 0x01, 0x9d });
    CHECK (bytes.size() == 22 + 413);
}

TEST_CASE ("What is written is read back as the same file", "[io][SmfWriter][SmfReader]")
{
    const auto file = fileWithEverything();
    const auto read = io::SmfReader::read (written (file));

    INFO (read.message.toStdString());
    REQUIRE (read.wasRead);
    CHECK (read.skippedChunks == 0);
    CHECK (read.file == file);

    SECTION ("and written again as the same bytes")
    {
        CHECK (written (read.file) == written (file));
    }

    SECTION ("also a type 0 file, and one with SMPTE time")
    {
        io::SmfFile other;
        other.format = 0;
        other.division = 0xe728;
        other.tracks.push_back (file.tracks[1]);

        const auto readOther = io::SmfReader::read (written (other));
        REQUIRE (readOther.wasRead);
        CHECK (readOther.file == other);
    }
}

TEST_CASE ("A file in the writer's form is read and written back byte for byte", "[io][SmfWriter][SmfReader]")
{
    const Bytes file {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x03, 0xc0,
        'M', 'T', 'r', 'k', 0, 0, 0, 20,
        0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
        0x00, 0xff, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x9e, 0x00, 0xff, 0x2f, 0x00,
        'M', 'T', 'r', 'k', 0, 0, 0, 33,
        0x00, 0x90, 0x3c, 0x64,
        0x87, 0x40, 0x3c, 0x00,
        0x00, 0x3c, 0x64,
        0x00, 0x80, 0x3c, 0x40,
        0x00, 0xf0, 0x03, 0x7e, 0x01, 0xf7,
        0x00, 0x80, 0x3e, 0x40,
        0x00, 0xb0, 0x07, 0x7f,
        0x00, 0xff, 0x2f, 0x00
    };

    const auto read = io::SmfReader::read (file);
    INFO (read.message.toStdString());
    REQUIRE (read.wasRead);

    CHECK (written (read.file) == file);
}

TEST_CASE ("A file that would be no Standard MIDI File is not written", "[io][SmfWriter]")
{
    SECTION ("a type that there is not")
    {
        io::SmfFile file;
        file.format = 3;
        CHECK (refusalOf (file) == "A MIDI file of type 3 cannot be written; there are only the types 0, 1 and 2.");

        file.format = -1;
        CHECK (refusalOf (file) == "A MIDI file of type -1 cannot be written; there are only the types 0, 1 and 2.");
    }

    SECTION ("a type 0 file without a track, or with two")
    {
        io::SmfFile file;
        file.format = 0;
        CHECK (refusalOf (file) == "A MIDI file of type 0 has exactly one track, not 0.");

        file.tracks.resize (2);
        CHECK (refusalOf (file) == "A MIDI file of type 0 has exactly one track, not 2.");
    }

    SECTION ("more tracks than the header can say")
    {
        io::SmfFile file;
        file.tracks.resize (65535);
        CHECK (io::SmfWriter::write (file).wasWritten);

        file.tracks.resize (65536);
        CHECK (refusalOf (file) == "A MIDI file cannot have 65536 tracks; 65535 is the most.");
    }
}

TEST_CASE ("A track whose ticks a file cannot say is not written", "[io][SmfWriter]")
{
    const auto reason = [] (std::vector<io::SmfEvent> events, std::int64_t endTick) {
        return refusalOf (fileOfOneTrack (std::move (events), endTick));
    };

    const auto note = [] (std::int64_t tick) { return channelMessage (tick, 0x90, { 0x3c, 0x64 }); };

    CHECK (reason ({ note (-1) }, 0) == "The MIDI file cannot be written: in track 1, event 1 is at a tick below 0.");
    CHECK (reason ({ note (10), note (9) }, 10) == "The MIDI file cannot be written: in track 1, event 2 is at a tick before the event before it.");
    CHECK (reason ({ note (10) }, 9) == "The MIDI file cannot be written: in track 1, its end is at a tick before the event before it.");
    CHECK (reason ({}, -1) == "The MIDI file cannot be written: in track 1, its end is at a tick below 0.");

    CHECK (reason ({ note (268435456) }, 268435456)
           == "The MIDI file cannot be written: in track 1, event 1 is further from the event before it than a file can say.");
    CHECK (reason ({ note (1), note (268435457) }, 268435457)
           == "The MIDI file cannot be written: in track 1, event 2 is further from the event before it than a file can say.");
    CHECK (reason ({ note (1) }, 268435457)
           == "The MIDI file cannot be written: in track 1, its end is further from the event before it than a file can say.");

    // The message names the track among the file's tracks.
    io::SmfFile file;
    file.tracks.resize (2);
    file.tracks[1].events.push_back (note (-1));
    CHECK (refusalOf (file) == "The MIDI file cannot be written: in track 2, event 1 is at a tick below 0.");
}

TEST_CASE ("An event that is none of a file's is not written", "[io][SmfWriter]")
{
    const auto reason = [] (const io::SmfEvent& event) {
        const auto message = refusalOf (fileOfOneTrack ({ event }, 0));
        const std::string start = "The MIDI file cannot be written: in track 1, event 1 ";
        REQUIRE (message.substr (0, start.size()) == start);
        return message.substr (start.size());
    };

    SECTION ("a channel message with the wrong number of data bytes")
    {
        CHECK (reason (channelMessage (0, 0x90, { 0x3c })) == "is a channel message with another number of data bytes than its status takes.");
        CHECK (reason (channelMessage (0, 0x90, { 0x3c, 0x64, 0x00 })) == "is a channel message with another number of data bytes than its status takes.");
        CHECK (reason (channelMessage (0, 0xc0, { 0x05, 0x00 })) == "is a channel message with another number of data bytes than its status takes.");
        CHECK (reason (channelMessage (0, 0xd0, {})) == "is a channel message with another number of data bytes than its status takes.");
    }

    SECTION ("a channel message with a data byte above 127")
    {
        CHECK (reason (channelMessage (0, 0x90, { 0x80, 0x64 })) == "is a channel message with a data byte above 127.");
        CHECK (reason (channelMessage (0, 0xb0, { 0x07, 0xff })) == "is a channel message with a data byte above 127.");
    }

    SECTION ("a status byte that is no event's")
    {
        CHECK (reason (channelMessage (0, 0x00, {})) == "has a status byte that no event of a file has.");
        CHECK (reason (channelMessage (0, 0x7f, { 0x3c, 0x64 })) == "has a status byte that no event of a file has.");
        CHECK (reason (channelMessage (0, 0xf8, {})) == "has a status byte that no event of a file has.");
        CHECK (reason (channelMessage (0, 0xf1, { 0x00 })) == "has a status byte that no event of a file has.");
    }

    SECTION ("a meta event of a type that there is not, and End of Track")
    {
        CHECK (reason (metaEvent (0, 0x80, {})) == "is a meta event of a type above 127.");
        CHECK (reason (metaEvent (0, 0x2f, {})) == "is End of Track, which is the track's endTick and no event.");
    }
}

TEST_CASE ("A refused file leaves nothing written", "[io][SmfWriter]")
{
    // The second track is at fault; nothing of the header or the first track
    // is given back.
    auto file = fileWithEverything();
    file.tracks[1].events.push_back (channelMessage (0, 0x90, { 0x3c, 0x64 }));

    const auto result = io::SmfWriter::write (file);
    CHECK_FALSE (result.wasWritten);
    CHECK (result.bytes.empty());
    CHECK (result.message == "The MIDI file cannot be written: in track 2, event 16 is at a tick before the event before it.");
}
