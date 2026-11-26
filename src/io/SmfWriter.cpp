#include "io/SmfWriter.h"

#include <cstddef>
#include <optional>

namespace io
{

namespace
{

    using Bytes = std::vector<std::uint8_t>;

    // What is wrong with a track, or nothing.
    using Problem = std::optional<juce::String>;

    void appendBigEndian (Bytes& out, std::uint32_t value, int count)
    {
        for (int shift = (count - 1) * 8; shift >= 0; shift -= 8)
            out.push_back (static_cast<std::uint8_t> ((value >> shift) & 0xff));
    }

    // A variable-length quantity in as few bytes as it takes: seven bits a
    // byte, the highest first, the top bit set on every byte but the last.
    // `value` is at most SmfWriter::largestVariableLength.
    void appendVariableLength (Bytes& out, std::uint32_t value)
    {
        for (int shift = 21; shift > 0; shift -= 7)
            if ((value >> shift) != 0)
                out.push_back (static_cast<std::uint8_t> (((value >> shift) & 0x7f) | 0x80));

        out.push_back (static_cast<std::uint8_t> (value & 0x7f));
    }

    void appendBytes (Bytes& out, const Bytes& bytes)
    {
        out.insert (out.end(), bytes.begin(), bytes.end());
    }

    // What of an event cannot be written, or nothing.
    Problem problemOf (const SmfEvent& event)
    {
        if (event.isChannelMessage())
        {
            if (static_cast<int> (event.bytes.size()) != SmfEvent::channelDataSize (event.status))
                return "is a channel message with another number of data bytes than its status takes";

            for (const auto data : event.bytes)
                if (data > 0x7f)
                    return "is a channel message with a data byte above 127";

            return {};
        }

        if (! event.isSysex() && ! event.isEscape() && ! event.isMeta())
            return "has a status byte that no event of a file has";

        if (event.isMeta() && event.metaType > 0x7f)
            return "is a meta event of a type above 127";

        if (event.isMeta() && event.metaType == SmfEvent::endOfTrackMetaType)
            return "is End of Track, which is the track's endTick and no event";

        if (static_cast<std::int64_t> (event.bytes.size()) > SmfWriter::largestVariableLength)
            return "has a payload longer than a file can say";

        return {};
    }

    // The bytes of one MTrk chunk after its length.
    Problem writeTrackBody (const SmfTrack& track, Bytes& out)
    {
        std::int64_t lastTick = 0;
        std::uint8_t runningStatus = 0;

        // The step to `tick` from the event before; it is checked here
        // because both the events and End of Track take it.
        const auto appendDelta = [&] (std::int64_t tick) -> Problem {
            if (tick < lastTick)
                return lastTick == 0 ? "is at a tick below 0" : "is at a tick before the event before it";

            if (tick - lastTick > SmfWriter::largestVariableLength)
                return "is further from the event before it than a file can say";

            appendVariableLength (out, static_cast<std::uint32_t> (tick - lastTick));
            lastTick = tick;
            return {};
        };

        for (std::size_t i = 0; i < track.events.size(); ++i)
        {
            const auto& event = track.events[i];
            auto problem = problemOf (event);

            if (! problem)
                problem = appendDelta (event.tick);

            if (problem)
                return "event " + juce::String (i + 1) + " " + *problem;

            if (event.isChannelMessage())
            {
                if (event.status != runningStatus)
                    out.push_back (event.status);

                appendBytes (out, event.bytes);
                runningStatus = event.status;
                continue;
            }

            out.push_back (event.status);

            if (event.isMeta())
                out.push_back (event.metaType);

            appendVariableLength (out, static_cast<std::uint32_t> (event.bytes.size()));
            appendBytes (out, event.bytes);
            runningStatus = 0;
        }

        if (const auto problem = appendDelta (track.endTick))
            return "its end " + *problem;

        out.push_back (SmfEvent::metaStatus);
        out.push_back (SmfEvent::endOfTrackMetaType);
        out.push_back (0);

        return {};
    }

    SmfWriter::Result refused (const juce::String& message)
    {
        SmfWriter::Result result;
        result.message = message;
        return result;
    }

} // namespace

SmfWriter::Result SmfWriter::write (const SmfFile& file)
{
    if (file.format < 0 || file.format > 2)
        return refused ("A MIDI file of type " + juce::String (file.format) + " cannot be written; there are only the types 0, 1 and 2.");

    if (file.format == 0 && file.tracks.size() != 1)
        return refused ("A MIDI file of type 0 has exactly one track, not " + juce::String (file.tracks.size()) + ".");

    if (file.tracks.size() > 0xffff)
        return refused ("A MIDI file cannot have " + juce::String (file.tracks.size()) + " tracks; 65535 is the most.");

    Result result;
    auto& out = result.bytes;

    appendBigEndian (out, 0x4d546864, 4); // "MThd"
    appendBigEndian (out, 6, 4);
    appendBigEndian (out, static_cast<std::uint32_t> (file.format), 2);
    appendBigEndian (out, static_cast<std::uint32_t> (file.tracks.size()), 2);
    appendBigEndian (out, file.division, 2);

    for (std::size_t i = 0; i < file.tracks.size(); ++i)
    {
        Bytes body;
        auto problem = writeTrackBody (file.tracks[i], body);

        if (! problem && body.size() > 0xffffffffu)
            problem = "it has more bytes than a chunk's length can say";

        if (problem)
            return refused ("The MIDI file cannot be written: in track " + juce::String (i + 1) + ", " + *problem + ".");

        appendBigEndian (out, 0x4d54726b, 4); // "MTrk"
        appendBigEndian (out, static_cast<std::uint32_t> (body.size()), 4);
        appendBytes (out, body);
    }

    result.wasWritten = true;
    return result;
}

} // namespace io
