#pragma once

#include "TrafficPattern.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace miditiming
{

struct ReferenceMessage
{
    std::int64_t offsetNanoseconds = 0; // from the file's tick 0, through its tempo map
    std::array<std::uint8_t, 3> bytes {};
    std::uint8_t size = 0;
};

// The reference for listen mode: a Standard MIDI File flattened into one
// time-ordered list of channel messages, which is what the app's engine
// meant to send (the SMF export is rendered by the same Kernel). Tracks are
// merged in file order, so messages at one tick keep the order export gave
// them. SysEx is not compared and is only counted; meta events are dropped.
class ReferenceFile
{
public:
    // Reads the file; on failure returns false and sets error.
    bool read (const std::string& path, std::string& error);

    const std::vector<ReferenceMessage>& messages() const { return list; }
    int sysexSkipped() const { return sysex; }
    int trackCount() const { return tracks; }

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
};

} // namespace miditiming
