#include "ReferenceFile.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace miditiming
{

namespace
{
    // A cursor over the file's bytes. Reading past the end sets failed and
    // returns 0, so the parser checks once per event instead of per byte.
    struct ByteReader
    {
        const std::vector<std::uint8_t>& bytes;
        std::size_t position = 0;
        std::size_t end = 0;
        bool failed = false;

        bool atEnd() const { return position >= end; }

        std::uint8_t byte()
        {
            if (position >= end)
            {
                failed = true;
                return 0;
            }
            return bytes[position++];
        }

        std::uint32_t bigEndian (int count)
        {
            std::uint32_t value = 0;
            for (int i = 0; i < count; ++i)
                value = (value << 8) | byte();
            return value;
        }

        // SMF variable-length quantity: seven bits per byte, at most four bytes.
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

        void skip (std::size_t count)
        {
            if (count > end - position)
                failed = true;
            else
                position += count;
        }
    };

    struct TempoChange
    {
        std::int64_t tick = 0;
        std::int64_t microsecondsPerQuarter = 0;
    };

    // One MTrk chunk: appends its channel messages in file order, collects
    // tempo events, counts SysEx. Returns false if the chunk is malformed.
    bool readTrack (ByteReader in, std::vector<ReferenceMessage>& messages, std::vector<TempoChange>& tempos, int& sysex)
    {
        std::int64_t tick = 0;
        std::uint8_t runningStatus = 0;

        while (! in.atEnd())
        {
            tick += in.variableLength();
            auto status = in.byte();
            if (in.failed)
                return false;

            if (status == 0xff) // meta event
            {
                const auto type = in.byte();
                const auto length = in.variableLength();
                if (in.failed)
                    return false;

                if (type == 0x2f) // End of Track
                    return true;

                if (type == 0x51 && length == 3)
                    tempos.push_back ({ tick, static_cast<std::int64_t> (in.bigEndian (3)) });
                else
                    in.skip (length);

                runningStatus = 0;
            }
            else if (status == 0xf0 || status == 0xf7) // SysEx, or a continuation / escape
            {
                if (status == 0xf0)
                    ++sysex;
                in.skip (in.variableLength());
                runningStatus = 0;
            }
            else
            {
                ReferenceMessage m;
                m.tick = tick;

                if (status < 0x80) // running status: this byte is already the first data byte
                {
                    if (runningStatus == 0)
                        return false;
                    m.bytes[1] = status;
                    status = runningStatus;
                }
                else if (status < 0xf0)
                {
                    m.bytes[1] = in.byte();
                }
                else
                {
                    return false; // no other system message may appear in a file
                }

                const auto kind = status & 0xf0;
                m.bytes[0] = status;
                m.size = (kind == 0xc0 || kind == 0xd0) ? 2 : 3;
                if (m.size == 3)
                    m.bytes[2] = in.byte();

                if (m.bytes[1] > 0x7f || m.bytes[2] > 0x7f)
                    return false;

                runningStatus = status;
                messages.push_back (m);
            }

            if (in.failed)
                return false;
        }

        return true; // a track without End of Track: take what it holds
    }
} // namespace

bool ReferenceFile::read (const std::string& path, std::string& error)
{
    std::ifstream stream (path, std::ios::binary);
    if (! stream)
    {
        error = "cannot open " + path;
        return false;
    }

    const std::vector<std::uint8_t> bytes { std::istreambuf_iterator<char> (stream), std::istreambuf_iterator<char>() };
    const auto unreadable = [&] {
        error = path + " is not a readable Standard MIDI File";
        return false;
    };

    ByteReader in { bytes, 0, bytes.size() };

    if (in.bigEndian (4) != 0x4d546864) // "MThd"
        return unreadable();

    const auto headerLength = in.bigEndian (4);
    const auto format = in.bigEndian (2);
    in.skip (2); // the track count; the chunks themselves are counted below
    const auto fileDivision = in.bigEndian (2);
    if (in.failed || headerLength < 6)
        return unreadable();
    in.skip (headerLength - 6);

    if (format > 1)
    {
        error = path + " is a type 2 Standard MIDI File; only types 0 and 1 have one timeline";
        return false;
    }

    if ((fileDivision & 0x8000) != 0 || fileDivision == 0)
    {
        error = path + " uses SMPTE time division, not ticks per quarter note";
        return false;
    }

    list.clear();
    sysex = 0;
    tracks = 0;
    std::vector<TempoChange> tempos;

    while (! in.failed && ! in.atEnd())
    {
        const auto type = in.bigEndian (4);
        const auto length = in.bigEndian (4);
        const auto start = in.position;
        in.skip (length);
        if (in.failed)
            return unreadable();

        if (type != 0x4d54726b) // "MTrk"; other chunks are to be ignored
            continue;

        if (! readTrack ({ bytes, start, start + length }, list, tempos, sysex))
            return unreadable();
        ++tracks;
    }

    if (list.empty())
    {
        error = path + " contains no channel messages";
        return false;
    }

    // Stable: at one tick, track order, then the file's order within a track.
    const auto byTick = [] (const auto& a, const auto& b) { return a.tick < b.tick; };
    std::stable_sort (list.begin(), list.end(), byTick);
    std::stable_sort (tempos.begin(), tempos.end(), byTick);

    division = static_cast<int> (fileDivision);
    tempoChanges.clear();
    for (const auto& t : tempos)
        tempoChanges.emplace_back (t.tick, t.microsecondsPerQuarter);

    for (auto& m : list)
        m.offsetNanoseconds = nanosecondsAt (m.tick);

    return true;
}

// Ticks to nanoseconds through the tempo map, in whole numbers: elapsed holds
// microseconds x ticks-per-quarter and is divided only at the end, so nothing
// accumulates. 120 bpm until the first tempo event, as the SMF specification
// says. Walks the tempo map from the start each time; tempo maps are short.
std::int64_t ReferenceFile::nanosecondsAt (std::int64_t tick) const
{
    std::int64_t elapsed = 0, segmentStart = 0, microsecondsPerQuarter = 500000;

    for (const auto& [changeTick, changeTempo] : tempoChanges)
    {
        if (changeTick > tick)
            break;

        elapsed += (changeTick - segmentStart) * microsecondsPerQuarter;
        segmentStart = changeTick;
        microsecondsPerQuarter = changeTempo;
    }

    const auto scaled = elapsed + (tick - segmentStart) * microsecondsPerQuarter;
    return (scaled * 1000 + division / 2) / division;
}

bool ReferenceFile::writePattern (const TrafficPattern& pattern, double bpm, const std::string& path, std::string& error)
{
    constexpr int ppq = 960;
    const double microsecondsPerQuarter = 60.0e6 / bpm;

    if (std::abs (microsecondsPerQuarter - std::round (microsecondsPerQuarter)) > 1.0e-9)
    {
        error = "at this tempo a quarter note is not a whole number of microseconds, so an SMF cannot hold it "
                "exactly; use e.g. --bpm 128";
        return false;
    }

    const double nanosecondsPerTick = microsecondsPerQuarter * 1000.0 / ppq;

    juce::MidiMessageSequence conductor;
    conductor.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (std::lround (microsecondsPerQuarter))));
    conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4));

    juce::MidiMessageSequence events;
    for (const auto& m : pattern.messages())
    {
        const auto ticks = static_cast<double> (m.offsetNanoseconds) / nanosecondsPerTick;
        if (std::abs (ticks - std::round (ticks)) > 1.0e-6)
        {
            error = "a pattern message does not fall on a whole tick at this tempo";
            return false;
        }

        const auto tick = std::round (ticks);
        events.addEvent (m.size == 2 ? juce::MidiMessage (m.bytes[0], m.bytes[1], tick)
                                     : juce::MidiMessage (m.bytes[0], m.bytes[1], m.bytes[2], tick));
    }

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ppq);
    midi.addTrack (conductor);
    midi.addTrack (events);

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (path));
    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk() || ! midi.writeTo (stream, 1))
    {
        error = "cannot write " + path;
        return false;
    }

    return true;
}

} // namespace miditiming
