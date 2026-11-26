// io::SmfReader: the bytes of a Standard MIDI File into an SmfFile, every
// event where and in the order in which the file has it
// (docs/RENDERING.md -> SMF bytes). The files are written out here byte by
// byte, by hand, and not made by SmfWriter.

#include <catch2/catch_test_macros.hpp>

#include "io/SmfReader.h"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{

using Bytes = std::vector<std::uint8_t>;

Bytes joined (std::initializer_list<Bytes> parts)
{
    Bytes all;

    for (const auto& part : parts)
        all.insert (all.end(), part.begin(), part.end());

    return all;
}

// An MThd chunk of six bytes.
Bytes header (int format, int trackCount, int division)
{
    return { 'M', 'T', 'h', 'd', 0, 0, 0, 6,
             0, static_cast<std::uint8_t> (format),
             static_cast<std::uint8_t> (trackCount >> 8), static_cast<std::uint8_t> (trackCount & 0xff),
             static_cast<std::uint8_t> (division >> 8), static_cast<std::uint8_t> (division & 0xff) };
}

// A chunk of the given type around `body`; the bodies here are shorter than
// 256 bytes.
Bytes chunk (const char* type, const Bytes& body)
{
    REQUIRE (body.size() < 256);

    Bytes bytes { static_cast<std::uint8_t> (type[0]), static_cast<std::uint8_t> (type[1]),
                  static_cast<std::uint8_t> (type[2]), static_cast<std::uint8_t> (type[3]),
                  0, 0, 0, static_cast<std::uint8_t> (body.size()) };
    bytes.insert (bytes.end(), body.begin(), body.end());

    return bytes;
}

Bytes track (const Bytes& body)
{
    return chunk ("MTrk", body);
}

// A type 1 file at 960 ticks a quarter with one track of this body.
Bytes fileOfOneTrack (const Bytes& body)
{
    return joined ({ header (1, 1, 960), track (body) });
}

const Bytes endOfTrack { 0x00, 0xff, 0x2f, 0x00 };

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

// The events of the one track of a file that must be readable.
std::vector<io::SmfEvent> eventsOf (const Bytes& trackBody)
{
    const auto result = io::SmfReader::read (fileOfOneTrack (trackBody));
    INFO (result.message.toStdString());
    REQUIRE (result.wasRead);
    REQUIRE (result.file.tracks.size() == 1);

    return result.file.tracks[0].events;
}

// The message with which a file is refused; the test fails when it is read.
std::string refusalOf (const Bytes& file)
{
    const auto result = io::SmfReader::read (file);
    CHECK_FALSE (result.wasRead);
    CHECK (result.file == io::SmfFile {});
    CHECK_FALSE (result.message.isEmpty());

    return result.message.toStdString();
}

std::string refusalOfTrack (const Bytes& trackBody)
{
    return refusalOf (fileOfOneTrack (trackBody));
}

// What is wrong in the one track of a file that must be refused: the message
// after "in track 1, at byte N, ".
std::string reason (const Bytes& trackBody)
{
    const auto message = refusalOfTrack (trackBody);
    const auto byte = message.find ("at byte ");
    REQUIRE (byte != std::string::npos);

    return message.substr (message.find (", ", byte) + 2);
}

} // namespace

TEST_CASE ("A file is read as its header and its tracks say", "[io][SmfReader]")
{
    const auto file = joined ({
        header (1, 2, 480),
        // tempo 500,000 microseconds at tick 0, End of Track at tick 480
        track ({ 0x00, 0xff, 0x51, 0x03, 0x07, 0xa1, 0x20,
                 0x83, 0x60, 0xff, 0x2f, 0x00 }),
        // note 60 on channel 1 from tick 0 to tick 96, End of Track at 96
        track ({ 0x00, 0x90, 0x3c, 0x64,
                 0x60, 0x80, 0x3c, 0x40,
                 0x00, 0xff, 0x2f, 0x00 }),
    });

    const auto result = io::SmfReader::read (file);

    REQUIRE (result.wasRead);
    CHECK (result.message.isEmpty());
    CHECK (result.skippedChunks == 0);
    CHECK (result.file.format == 1);
    CHECK (result.file.division == 480);
    CHECK_FALSE (result.file.usesSmpteTime());
    REQUIRE (result.file.tracks.size() == 2);

    const auto& conductor = result.file.tracks[0];
    CHECK (conductor.events == std::vector<io::SmfEvent> { metaEvent (0, 0x51, { 0x07, 0xa1, 0x20 }) });
    CHECK (conductor.endTick == 480);

    const auto& notes = result.file.tracks[1];
    CHECK (notes.events == std::vector<io::SmfEvent> { channelMessage (0, 0x90, { 0x3c, 0x64 }), channelMessage (96, 0x80, { 0x3c, 0x40 }) });
    CHECK (notes.endTick == 96);
}

TEST_CASE ("Events at one tick stay in the order of the file", "[io][SmfReader]")
{
    // The reason this reader exists: juce::MidiFile::readFrom would give both
    // tracks below as note-off, note-on.
    SECTION ("a note-on before the note-off of its pitch")
    {
        const auto events = eventsOf ({ 0x00, 0x90, 0x3c, 0x64,
                                        0x00, 0x80, 0x3c, 0x40,
                                        0x00, 0xff, 0x2f, 0x00 });

        CHECK (events == std::vector<io::SmfEvent> { channelMessage (0, 0x90, { 0x3c, 0x64 }), channelMessage (0, 0x80, { 0x3c, 0x40 }) });
    }

    SECTION ("a note-off before the note-on of its pitch")
    {
        const auto events = eventsOf ({ 0x00, 0x80, 0x3c, 0x40,
                                        0x00, 0x90, 0x3c, 0x64,
                                        0x00, 0xff, 0x2f, 0x00 });

        CHECK (events == std::vector<io::SmfEvent> { channelMessage (0, 0x80, { 0x3c, 0x40 }), channelMessage (0, 0x90, { 0x3c, 0x64 }) });
    }

    SECTION ("a note between two controllers, a program change after its note")
    {
        const auto events = eventsOf ({ 0x0a, 0xb0, 0x20, 0x01,
                                        0x00, 0x90, 0x3c, 0x64,
                                        0x00, 0xb0, 0x00, 0x02,
                                        0x00, 0xc0, 0x05,
                                        0x00, 0xff, 0x2f, 0x00 });

        CHECK (events == std::vector<io::SmfEvent> { channelMessage (10, 0xb0, { 0x20, 0x01 }), channelMessage (10, 0x90, { 0x3c, 0x64 }), channelMessage (10, 0xb0, { 0x00, 0x02 }), channelMessage (10, 0xc0, { 0x05 }) });
    }
}

TEST_CASE ("A channel message without a status byte has the status of the one before", "[io][SmfReader]")
{
    SECTION ("with two data bytes; a note-on of velocity 0 stays a note-on")
    {
        const auto events = eventsOf ({ 0x00, 0x91, 0x3c, 0x64,
                                        0x10, 0x3e, 0x50,
                                        0x10, 0x3c, 0x00,
                                        0x00, 0xff, 0x2f, 0x00 });

        CHECK (events == std::vector<io::SmfEvent> { channelMessage (0, 0x91, { 0x3c, 0x64 }), channelMessage (16, 0x91, { 0x3e, 0x50 }), channelMessage (32, 0x91, { 0x3c, 0x00 }) });
    }

    SECTION ("with one data byte")
    {
        const auto events = eventsOf ({ 0x00, 0xc2, 0x05,
                                        0x01, 0x06,
                                        0x01, 0xd2, 0x40,
                                        0x01, 0x41,
                                        0x00, 0xff, 0x2f, 0x00 });

        CHECK (events == std::vector<io::SmfEvent> { channelMessage (0, 0xc2, { 0x05 }), channelMessage (1, 0xc2, { 0x06 }), channelMessage (2, 0xd2, { 0x40 }), channelMessage (3, 0xd2, { 0x41 }) });
    }

    SECTION ("also across a meta event and a SysEx event")
    {
        const auto events = eventsOf ({ 0x00, 0x90, 0x3c, 0x64,
                                        0x00, 0xff, 0x06, 0x01, 0x41,
                                        0x00, 0x3e, 0x64,
                                        0x00, 0xf0, 0x02, 0x7e, 0xf7,
                                        0x00, 0x40, 0x64,
                                        0x00, 0xff, 0x2f, 0x00 });

        REQUIRE (events.size() == 5);
        CHECK (events[2] == channelMessage (0, 0x90, { 0x3e, 0x64 }));
        CHECK (events[4] == channelMessage (0, 0x90, { 0x40, 0x64 }));
    }
}

TEST_CASE ("SysEx, escape and meta events are read with their bytes", "[io][SmfReader]")
{
    const auto events = eventsOf ({        // F0 7E 7F 09 01 F7: in the file, the length and what follows the F0
                                    0x00, 0xf0, 0x05, 0x7e, 0x7f, 0x09, 0x01, 0xf7,
        // a SysEx begun and not ended, and its continuation
                                    0x05, 0xf0, 0x02, 0x43, 0x12,
                                    0x05, 0xf7, 0x02, 0x00, 0xf7,
        // a marker "A", and a meta event without payload
                                    0x00, 0xff, 0x06, 0x01, 0x41,
                                    0x00, 0xff, 0x7f, 0x00,
                                    0x00, 0xff, 0x2f, 0x00 });

    REQUIRE (events.size() == 5);

    CHECK (events[0].isSysex());
    CHECK (events[0].tick == 0);
    CHECK (events[0].bytes == Bytes { 0x7e, 0x7f, 0x09, 0x01, 0xf7 });

    CHECK (events[1].isSysex());
    CHECK (events[1].tick == 5);
    CHECK (events[1].bytes == Bytes { 0x43, 0x12 });

    CHECK (events[2].isEscape());
    CHECK (events[2].tick == 10);
    CHECK (events[2].bytes == Bytes { 0x00, 0xf7 });

    CHECK (events[3] == metaEvent (10, 0x06, { 0x41 }));
    CHECK (events[4] == metaEvent (10, 0x7f, {}));
}

TEST_CASE ("A delta time of one to four bytes is read, and the ticks add up", "[io][SmfReader]")
{
    const auto result = io::SmfReader::read (fileOfOneTrack ({ 0x7f, 0xb0, 0x07, 0x01,                   // 127
                                                               0x81, 0x00, 0xb0, 0x07, 0x02,             // 128
                                                               0xff, 0x7f, 0xb0, 0x07, 0x03,             // 16,383
                                                               0x81, 0x80, 0x00, 0xb0, 0x07, 0x04,       // 16,384
                                                               0xff, 0xff, 0xff, 0x7f, 0xb0, 0x07, 0x05, // 268,435,455
                                                               0xff, 0xff, 0xff, 0x7f, 0xff, 0x2f, 0x00 }));

    REQUIRE (result.wasRead);
    const auto& track = result.file.tracks[0];
    REQUIRE (track.events.size() == 5);
    CHECK (track.events[0].tick == 127);
    CHECK (track.events[1].tick == 255);
    CHECK (track.events[2].tick == 16638);
    CHECK (track.events[3].tick == 33022);
    CHECK (track.events[4].tick == 268468477);
    CHECK (track.endTick == 536903932);
}

TEST_CASE ("The header's numbers are kept as they stand", "[io][SmfReader]")
{
    SECTION ("type 0")
    {
        const auto result = io::SmfReader::read (joined ({ header (0, 1, 96), track (endOfTrack) }));
        REQUIRE (result.wasRead);
        CHECK (result.file.format == 0);
        CHECK (result.file.division == 96);
    }

    SECTION ("type 2 and SMPTE time, which the import refuses and the reader does not")
    {
        // -25 frames a second, 40 ticks a frame
        const auto result = io::SmfReader::read (joined ({ header (2, 1, 0xe728), track (endOfTrack) }));
        REQUIRE (result.wasRead);
        CHECK (result.file.format == 2);
        CHECK (result.file.division == 0xe728);
        CHECK (result.file.usesSmpteTime());
    }

    SECTION ("a file without tracks")
    {
        const auto result = io::SmfReader::read (header (1, 0, 960));
        REQUIRE (result.wasRead);
        CHECK (result.file.tracks.empty());
    }

    SECTION ("an empty track")
    {
        const auto result = io::SmfReader::read (fileOfOneTrack ({ 0x83, 0x60, 0xff, 0x2f, 0x00 }));
        REQUIRE (result.wasRead);
        CHECK (result.file.tracks[0].events.empty());
        CHECK (result.file.tracks[0].endTick == 480);
    }
}

TEST_CASE ("A longer header and chunks that are no track are passed over", "[io][SmfReader]")
{
    const Bytes longHeader { 'M', 'T', 'h', 'd', 0, 0, 0, 8, 0, 1, 0, 1, 0x03, 0xc0, 0xaa, 0xbb };

    const auto result = io::SmfReader::read (joined ({ longHeader,
                                                       chunk ("XFIH", { 0x01, 0x02, 0x03 }),
                                                       track ({ 0x00, 0xc0, 0x05, 0x00, 0xff, 0x2f, 0x00 }),
                                                       chunk ("XFKM", {}) }));

    REQUIRE (result.wasRead);
    CHECK (result.skippedChunks == 2);
    CHECK (result.file.division == 960);
    REQUIRE (result.file.tracks.size() == 1);
    CHECK (result.file.tracks[0].events == std::vector<io::SmfEvent> { channelMessage (0, 0xc0, { 0x05 }) });
}

TEST_CASE ("Bytes that are no Standard MIDI File are refused", "[io][SmfReader]")
{
    CHECK (refusalOf ({}) == "This is no Standard MIDI File: it does not begin with an MThd chunk.");
    CHECK (refusalOf ({ 'M', 'T', 'h' }) == "This is no Standard MIDI File: it does not begin with an MThd chunk.");
    CHECK (refusalOf ({ 'R', 'I', 'F', 'F', 0, 0, 0, 6, 0, 1, 0, 0, 3, 0xc0 }) == "This is no Standard MIDI File: it does not begin with an MThd chunk.");

    // a header that ends early, one of five bytes, one that says it is longer than the file
    CHECK (refusalOf ({ 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 0 }) == "The header of the MIDI file is cut short.");
    CHECK (refusalOf ({ 'M', 'T', 'h', 'd', 0, 0, 0, 5, 0, 1, 0, 0, 3, 0xc0 }) == "The header of the MIDI file is cut short.");
    CHECK (refusalOf ({ 'M', 'T', 'h', 'd', 0, 0, 0, 7, 0, 1, 0, 0, 3, 0xc0 }) == "The header of the MIDI file is cut short.");

    CHECK (refusalOf (joined ({ header (3, 1, 960), track (endOfTrack) })) == "The MIDI file says it is of type 3; there are only the types 0, 1 and 2.");
}

TEST_CASE ("A file whose chunks are not whole, or not as many as it says, is refused", "[io][SmfReader]")
{
    SECTION ("bytes after the last chunk that are no chunk")
    {
        CHECK (refusalOf (joined ({ header (1, 1, 960), track (endOfTrack), { 'M', 'T', 'r' } }))
               == "The MIDI file ends with bytes that are no chunk, at byte 26.");
    }

    SECTION ("a chunk that says it is longer than the rest of the file")
    {
        auto file = fileOfOneTrack ({ 0x00, 0x90, 0x3c, 0x64, 0x00, 0xff, 0x2f, 0x00 });
        file.pop_back();

        CHECK (refusalOf (file) == "The MIDI file is cut short: the chunk at byte 14 says it is longer than the rest of the file.");
    }

    SECTION ("fewer tracks than the header says, and more")
    {
        CHECK (refusalOf (joined ({ header (1, 2, 960), track (endOfTrack) })) == "The header of the MIDI file says it has 2 tracks, and it has 1.");
        CHECK (refusalOf (joined ({ header (1, 1, 960), track (endOfTrack), track (endOfTrack) }))
               == "The header of the MIDI file says it has 1 tracks, and it has 2.");
    }
}

TEST_CASE ("A track that does not end as a track ends is refused", "[io][SmfReader]")
{
    // The track's bytes begin at byte 22 of the file: 14 of the header, 8 of
    // the chunk's type and length.
    CHECK (refusalOfTrack ({}) == "The MIDI file cannot be read: in track 1, at byte 22, the track has no End of Track.");
    CHECK (refusalOfTrack ({ 0x00, 0x90, 0x3c, 0x64 }) == "The MIDI file cannot be read: in track 1, at byte 26, the track has no End of Track.");
    CHECK (refusalOfTrack ({ 0x00, 0xff, 0x2f, 0x00, 0x00, 0x90, 0x3c, 0x64 }) == "The MIDI file cannot be read: in track 1, at byte 26, bytes follow End of Track.");
    CHECK (refusalOfTrack ({ 0x00, 0xff, 0x2f, 0x01, 0x00 }) == "The MIDI file cannot be read: in track 1, at byte 27, End of Track has a payload.");

    // The message names the track among the file's tracks.
    CHECK (refusalOf (joined ({ header (1, 2, 960), track (endOfTrack), track ({}) }))
           == "The MIDI file cannot be read: in track 2, at byte 34, the track has no End of Track.");
}

TEST_CASE ("An event that is cut short is refused", "[io][SmfReader]")
{
    CHECK (reason ({ 0x81 }) == "a delta time is cut short or has more than four bytes.");
    CHECK (reason ({ 0x81, 0x80, 0x80, 0x80, 0x00, 0xff, 0x2f, 0x00 }) == "a delta time is cut short or has more than four bytes.");
    CHECK (reason ({ 0x00 }) == "the track ends after a delta time.");
    CHECK (reason ({ 0x00, 0x90 }) == "a channel message is cut short.");
    CHECK (reason ({ 0x00, 0x90, 0x3c }) == "a channel message is cut short.");
    CHECK (reason ({ 0x00, 0x90, 0x3c, 0x64, 0x00, 0x3c }) == "a channel message is cut short.");
    CHECK (reason ({ 0x00, 0xc0 }) == "a channel message is cut short.");
    CHECK (reason ({ 0x00, 0xff }) == "a meta event is cut short.");
    CHECK (reason ({ 0x00, 0xff, 0x06 }) == "a meta event is cut short.");
    CHECK (reason ({ 0x00, 0xff, 0x06, 0x02, 0x41 }) == "a meta event is cut short.");
    CHECK (reason ({ 0x00, 0xff, 0x06, 0x81, 0x80, 0x80, 0x80, 0x00 }) == "a meta event is cut short.");
    CHECK (reason ({ 0x00, 0xf0 }) == "a SysEx event is cut short.");
    CHECK (reason ({ 0x00, 0xf0, 0x03, 0x7e, 0xf7 }) == "a SysEx event is cut short.");
    CHECK (reason ({ 0x00, 0xf7, 0x01 }) == "a SysEx event is cut short.");
}

TEST_CASE ("A byte that cannot stand where it stands is refused", "[io][SmfReader]")
{
    // a data byte first, with no channel message before it
    CHECK (reason ({ 0x00, 0x3c, 0x64, 0x00, 0xff, 0x2f, 0x00 }) == "a data byte stands where a status byte must, and no status is running.");
    CHECK (reason ({ 0x00, 0xff, 0x06, 0x00, 0x00, 0x3c, 0x64, 0x00, 0xff, 0x2f, 0x00 })
           == "a data byte stands where a status byte must, and no status is running.");

    // a status byte in the place of a data byte
    CHECK (reason ({ 0x00, 0x90, 0x3c, 0x90, 0x00, 0xff, 0x2f, 0x00 }) == "a channel message has the data byte 0x90.");
    CHECK (reason ({ 0x00, 0x90, 0xff, 0x2f, 0x00 }) == "a channel message has the data byte 0xFF.");
    CHECK (reason ({ 0x00, 0xc0, 0x80, 0x00, 0xff, 0x2f, 0x00 }) == "a channel message has the data byte 0x80.");

    // system messages that only a cable carries
    CHECK (reason ({ 0x00, 0xf8, 0x00, 0xff, 0x2f, 0x00 }) == "the status byte 0xF8 has no place in a file.");
    CHECK (reason ({ 0x00, 0xf1, 0x00, 0x00, 0xff, 0x2f, 0x00 }) == "the status byte 0xF1 has no place in a file.");
    CHECK (reason ({ 0x00, 0xfe, 0x00, 0xff, 0x2f, 0x00 }) == "the status byte 0xFE has no place in a file.");
}
