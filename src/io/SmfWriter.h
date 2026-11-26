#pragma once

#include "io/SmfFile.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <vector>

namespace io
{

// Writes an SmfFile as the bytes of a Standard MIDI File, every event where
// and in the order in which the SmfFile has it (docs/RENDERING.md -> SMF
// bytes). What SmfReader reads from these bytes is the SmfFile again.
//
// The bytes are in one form:
//  - an MThd chunk of six bytes, then one MTrk chunk per track;
//  - a channel message without its status byte when the event before it in
//    the track is a channel message of the same status (running status); a
//    meta or SysEx event ends that, as the specification says;
//  - every delta time and length in as few bytes as it takes;
//  - each track ends with End of Track at its endTick.
class SmfWriter
{
public:
    struct Result
    {
        bool wasWritten = false;

        // The file's bytes; empty when it was not written.
        std::vector<std::uint8_t> bytes;

        // Why the file was not written, naming the track and the event.
        // Empty when it was.
        juce::String message;
    };

    // The largest number a delta time or a length can be in a file: 28 bits.
    static constexpr std::int64_t largestVariableLength = 0x0fffffff;

    // Refused, with nothing written, is a file that would be no Standard
    // MIDI File, or another one than the SmfFile says:
    //  - a format that is not 0, 1 or 2; a type 0 file that has not exactly
    //    one track; more than 65,535 tracks;
    //  - a tick below 0 or before the event before it, an endTick before the
    //    last event, and a step from one event to the next of more than
    //    largestVariableLength ticks;
    //  - an event whose status is none of the four kinds (SmfEvent); a
    //    channel message with another number of data bytes than its status
    //    takes, or with one above 127; a metaType above 127; End of Track
    //    among the events; a payload longer than largestVariableLength;
    //  - a track of more bytes than a chunk's length can say.
    static Result write (const SmfFile& file);
};

} // namespace io
