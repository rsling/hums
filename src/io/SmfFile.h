#pragma once

#include "io/SmfTrack.h"

#include <cstdint>
#include <vector>

namespace io
{

// A Standard MIDI File as its bytes say it: the three numbers of its header
// and its tracks, in file order. SmfReader makes one from bytes and SmfWriter
// bytes from one; SmfImport and SmfExport work on this and never see a byte
// of the file.
//
// It holds what a file holds, not what Hums takes: a type 2 file and one with
// SMPTE time are read into it like any other, and refusing them is the
// import's work (docs/RENDERING.md -> SMF import policies).
//
// Std only, a plain value.
struct SmfFile
{
    // 0, 1 or 2.
    int format = 1;

    // The header's division, its sixteen bits as they stand. Without the top
    // bit it is the ticks of a quarter note; with it, SMPTE time.
    std::uint16_t division = 960;

    std::vector<SmfTrack> tracks;

    bool usesSmpteTime() const { return (division & 0x8000) != 0; }

    bool operator== (const SmfFile&) const = default;
};

} // namespace io
