#include "io/SmfReader.h"

#include <cstddef>
#include <optional>

namespace io
{

namespace
{

    constexpr std::uint32_t headerChunkType = 0x4d546864; // "MThd"
    constexpr std::uint32_t trackChunkType = 0x4d54726b;  // "MTrk"

    // A stretch of the file's bytes and a place in it. Reading past its end
    // sets `failed` and gives 0, so that an event is checked once and not
    // byte by byte. `position` counts from the start of the file, which is
    // what a message names.
    struct Cursor
    {
        std::span<const std::uint8_t> file;
        std::size_t position = 0;
        std::size_t end = 0;
        bool failed = false;

        bool atEnd() const { return position >= end; }
        std::size_t remaining() const { return end - position; }

        std::uint8_t byte()
        {
            if (atEnd())
            {
                failed = true;
                return 0;
            }

            return file[position++];
        }

        std::uint32_t bigEndian (int count)
        {
            std::uint32_t value = 0;

            for (int i = 0; i < count; ++i)
                value = (value << 8) | byte();

            return value;
        }

        // A variable-length quantity: seven bits a byte, the top bit saying
        // that another byte follows, four bytes at most.
        std::uint32_t variableLength()
        {
            std::uint32_t value = 0;

            for (int i = 0; i < 4; ++i)
            {
                const auto b = byte();
                value = (value << 7) | (b & 0x7fu);

                if ((b & 0x80) == 0)
                    return value;
            }

            failed = true;
            return 0;
        }

        // The next `count` bytes; none, and failed, when there are fewer.
        std::vector<std::uint8_t> take (std::size_t count)
        {
            if (count > remaining())
            {
                failed = true;
                return {};
            }

            const auto first = file.begin() + static_cast<std::ptrdiff_t> (position);
            position += count;

            return { first, first + static_cast<std::ptrdiff_t> (count) };
        }
    };

    // What is wrong with a track, or nothing.
    using Problem = std::optional<juce::String>;

    juce::String hexByte (std::uint8_t byte)
    {
        return "0x" + juce::String::toHexString (static_cast<int> (byte)).paddedLeft ('0', 2).toUpperCase();
    }

    // One MTrk chunk, of which `in` holds the bytes after the chunk's length.
    Problem readTrack (Cursor& in, SmfTrack& track)
    {
        std::int64_t tick = 0;
        std::uint8_t runningStatus = 0;

        while (! in.atEnd())
        {
            tick += in.variableLength();

            if (in.failed)
                return "a delta time is cut short or has more than four bytes";

            SmfEvent event;
            event.tick = tick;

            const auto first = in.byte();

            if (in.failed)
                return "the track ends after a delta time";

            if (first == SmfEvent::metaStatus)
            {
                event.status = first;
                event.metaType = in.byte();
                const auto length = in.variableLength();
                event.bytes = in.take (length);

                if (in.failed)
                    return "a meta event is cut short";

                if (event.metaType == SmfEvent::endOfTrackMetaType)
                {
                    if (length != 0)
                        return "End of Track has a payload";

                    if (! in.atEnd())
                        return "bytes follow End of Track";

                    track.endTick = tick;
                    return {};
                }
            }
            else if (first == SmfEvent::sysexStatus || first == SmfEvent::escapeStatus)
            {
                event.status = first;
                event.bytes = in.take (in.variableLength());

                if (in.failed)
                    return "a SysEx event is cut short";
            }
            else if (first >= 0xf0)
            {
                return "the status byte " + hexByte (first) + " has no place in a file";
            }
            else
            {
                if (first >= 0x80)
                {
                    event.status = first;
                    event.bytes.push_back (in.byte());
                }
                else
                {
                    // Running status: this is already the first data byte.
                    if (runningStatus == 0)
                        return "a data byte stands where a status byte must, and no status is running";

                    event.status = runningStatus;
                    event.bytes.push_back (first);
                }

                if (SmfEvent::channelDataSize (event.status) == 2)
                    event.bytes.push_back (in.byte());

                if (in.failed)
                    return "a channel message is cut short";

                for (const auto data : event.bytes)
                    if (data > 0x7f)
                        return "a channel message has the data byte " + hexByte (data);

                runningStatus = event.status;
            }

            track.events.push_back (std::move (event));
        }

        return "the track has no End of Track";
    }

    SmfReader::Result refused (const juce::String& message)
    {
        SmfReader::Result result;
        result.message = message;
        return result;
    }

} // namespace

SmfReader::Result SmfReader::read (std::span<const std::uint8_t> bytes)
{
    Cursor in { bytes, 0, bytes.size() };

    if (in.bigEndian (4) != headerChunkType || in.failed)
        return refused ("This is no Standard MIDI File: it does not begin with an MThd chunk.");

    const auto headerLength = in.bigEndian (4);
    const auto format = in.bigEndian (2);
    const auto trackCount = in.bigEndian (2);
    const auto division = in.bigEndian (2);

    if (in.failed || headerLength < 6 || headerLength - 6 > in.remaining())
        return refused ("The header of the MIDI file is cut short.");

    in.position += headerLength - 6;

    if (format > 2)
        return refused ("The MIDI file says it is of type " + juce::String (format) + "; there are only the types 0, 1 and 2.");

    Result result;
    result.file.format = static_cast<int> (format);
    result.file.division = static_cast<std::uint16_t> (division);

    while (! in.atEnd())
    {
        const auto chunkStart = in.position;

        if (in.remaining() < 8)
            return refused ("The MIDI file ends with bytes that are no chunk, at byte " + juce::String (chunkStart) + ".");

        const auto type = in.bigEndian (4);
        const auto length = in.bigEndian (4);

        if (length > in.remaining())
            return refused ("The MIDI file is cut short: the chunk at byte " + juce::String (chunkStart)
                            + " says it is longer than the rest of the file.");

        const auto chunkEnd = in.position + length;

        if (type != trackChunkType)
        {
            ++result.skippedChunks;
            in.position = chunkEnd;
            continue;
        }

        Cursor chunk { bytes, in.position, chunkEnd };
        SmfTrack track;

        if (const auto problem = readTrack (chunk, track))
            return refused ("The MIDI file cannot be read: in track " + juce::String (result.file.tracks.size() + 1)
                            + ", at byte " + juce::String (chunk.position) + ", " + *problem + ".");

        result.file.tracks.push_back (std::move (track));
        in.position = chunkEnd;
    }

    if (result.file.tracks.size() != trackCount)
        return refused ("The header of the MIDI file says it has " + juce::String (trackCount) + " tracks, and it has "
                        + juce::String (result.file.tracks.size()) + ".");

    result.wasRead = true;
    return result;
}

} // namespace io
