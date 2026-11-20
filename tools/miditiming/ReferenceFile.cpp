#include "ReferenceFile.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

namespace miditiming
{

bool ReferenceFile::read (const std::string& path, std::string& error)
{
    // getChildFile: a relative path is taken from the working directory.
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (path));
    juce::FileInputStream stream (file);

    if (! stream.openedOk())
    {
        error = "cannot open " + path;
        return false;
    }

    juce::MidiFile midi;
    // No invented note-offs: an unmatched note-on is what the file says.
    if (! midi.readFrom (stream, false))
    {
        error = path + " is not a readable Standard MIDI File";
        return false;
    }

    midi.convertTimestampTicksToSeconds();

    list.clear();
    sysex = 0;
    tracks = midi.getNumTracks();

    for (int t = 0; t < tracks; ++t)
    {
        const auto& sequence = *midi.getTrack (t);
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            const auto& m = sequence.getEventPointer (i)->message;
            if (m.isSysEx()) { ++sysex; continue; }

            const auto* data = m.getRawData();
            const auto size = m.getRawDataSize();
            if (size < 1 || size > 3 || data[0] < 0x80 || data[0] >= 0xf0)
                continue; // meta events and anything else that is not a channel message

            ReferenceMessage r;
            r.offsetNanoseconds = std::llround (m.getTimeStamp() * 1.0e9);
            r.size = static_cast<std::uint8_t> (size);
            for (int b = 0; b < size; ++b)
                r.bytes[static_cast<std::size_t> (b)] = data[b];
            list.push_back (r);
        }
    }

    // Stable: equal times keep track order, then file order within a track.
    std::stable_sort (list.begin(), list.end(),
                      [] (const ReferenceMessage& a, const ReferenceMessage& b) { return a.offsetNanoseconds < b.offsetNanoseconds; });

    if (list.empty())
    {
        error = path + " contains no channel messages";
        return false;
    }

    return true;
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
