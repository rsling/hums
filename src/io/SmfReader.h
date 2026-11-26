#pragma once

#include "io/SmfFile.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <span>

namespace io
{

// Reads the bytes of a Standard MIDI File into an SmfFile, every event where
// and in the order in which the file has it (docs/RENDERING.md -> SMF bytes).
//
// It is ours because juce::MidiFile::readFrom cannot be used for it: that
// reader sorts each track and then moves a note-off in front of a note-on of
// the same pitch on the same tick (reorderNoteOnsAfterNoteOffs in
// juce_MidiFile.cpp at the pinned tag, called for every track), and the
// import promises file order at equal ticks (docs/RENDERING.md -> Equal-tick
// order).
//
// It reads and decides nothing: which kinds of file Hums takes, and what
// becomes of each event, is SmfImport's.
class SmfReader
{
public:
    struct Result
    {
        // Whether the bytes were a Standard MIDI File that could be read to
        // its end.
        bool wasRead = false;

        // The file; empty when it was not read.
        SmfFile file;

        // Chunks that are no MTrk. A reader is to pass over them, and they
        // are counted so that the import can report them.
        int skippedChunks = 0;

        // Why the bytes were not read, in words for the user, with the track
        // and the byte at which reading stopped. Empty when they were read.
        juce::String message;
    };

    // Reads a whole file.
    //
    // Taken as the file means it, where that is beyond doubt:
    //  - running status, also where a meta or SysEx event stands between the
    //    two channel messages (the specification ends running status there;
    //    files that carry on exist, and they can be read in one way only);
    //  - a header longer than its six bytes, and chunks that are no MTrk.
    //
    // Refused, with nothing read:
    //  - bytes that do not begin with an MThd chunk of at least six bytes,
    //    or whose format is not 0, 1 or 2;
    //  - a chunk that is cut short, or bytes after the last chunk that are
    //    no chunk;
    //  - another number of MTrk chunks than the header says;
    //  - a track without End of Track, with bytes after it, or whose End of
    //    Track has a payload;
    //  - an event that is cut short, a delta time or length of more than
    //    four bytes;
    //  - a data byte where a status byte must stand and no status is
    //    running, a data byte above 127, and a status byte that has no place
    //    in a file (0xF1 to 0xF6, 0xF8 to 0xFE).
    static Result read (std::span<const std::uint8_t> bytes);
};

} // namespace io
