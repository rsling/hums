#pragma once

#include "TrafficPattern.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace miditiming
{

struct ReferenceMessage
{
    std::int64_t tick = 0;              // in the file's own ticks
    std::int64_t offsetNanoseconds = 0; // from the file's tick 0, through its tempo map
    std::array<std::uint8_t, 3> bytes {};
    std::uint8_t size = 0;
};

// The reference for listen mode: a Standard MIDI File flattened into one
// list of channel messages, which is what the app's engine meant to send (the
// SMF export is rendered by the same Kernel). SysEx is not compared and is
// only counted; meta events other than tempo are dropped.
//
// - Order: tracks are merged in file order, and the messages of one tick keep
//   exactly the order the file gives them. Listen mode checks arrivals against
//   that order, so the file is read here byte by byte and not through
//   juce::MidiFile, whose reader always moves a note-off in front of a
//   same-pitch note-on on the same tick (juce_MidiFile.cpp,
//   reorderNoteOnsAfterNoteOffs) and would turn a retrigger exported as
//   on, off, on into off, on, on.
// - Time: ticks become nanoseconds through the file's tempo events, which an
//   SMF holds as whole microseconds per quarter note. Hums's TempoMap realises
//   every tempo as exactly that number [D13], so for a Hums export these are
//   the engine's own times. A file from an app that plays 118 bpm as exactly
//   118 would read as drifting, by up to bpm x 0.0005 ms per minute.
class ReferenceFile
{
public:
    // Reads the file; on failure returns false and sets error.
    bool read (const std::string& path, std::string& error);

    const std::vector<ReferenceMessage>& messages() const { return list; }
    int sysexSkipped() const { return sysex; }
    int trackCount() const { return tracks; }

    // The file's time base, and the time of any tick through its tempo map:
    // what listen mode needs to place MIDI clock pulses (24 per quarter note).
    int ticksPerQuarter() const { return division; }
    std::int64_t nanosecondsAt (std::int64_t tick) const;

    // Writes a traffic pattern as a type 1 SMF (PPQ 960, one tempo event), so
    // listen mode can be checked against miditiming's own send mode, or a
    // file played by another app. Exact only when the tempo is a whole number
    // of microseconds per quarter (128 bpm is; 121 is not): returns false and
    // sets error otherwise, rather than writing a file that drifts.
    static bool writePattern (const TrafficPattern& pattern, double bpm, const std::string& path, std::string& error);

private:
    std::vector<ReferenceMessage> list;
    int sysex = 0;
    int tracks = 0;
    int division = 960;
    std::vector<std::pair<std::int64_t, std::int64_t>> tempoChanges; // (tick, microseconds per quarter), by tick
};

} // namespace miditiming
