#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace render
{

// What one clip plays in one pass of its loop, as plain data
// (docs/RENDERING.md -> Pipeline). SnapshotBuilder makes one per referenced
// clip on the message thread; the Kernel loops it inside each placement.
//
// A template holds only what is played: enabled content that begins inside
// the loop. Notes and events are two lists because the order at one tick
// keeps them apart (note-offs, then events, then note-ons); each list is in
// tick order, and at one tick in the order the clip has them as children.
//
// Std only, and fixed in size once built: reading it allocates nothing.
struct ClipTemplate
{
    struct Note
    {
        // The onset, 0 <= tick < loopLength.
        std::int64_t tick = 0;
        // At least 1. tick + length may lie past loopLength: the note then
        // plays on into the next pass of the loop.
        std::int64_t length = 0;
        int pitch = 0;
        int velocity = 0;
        int offVelocity = 0;
    };

    enum class EventKind
    {
        cc,              // a = controller, b = value
        pitchBend,       // a = value 0-16383, 8192 = centre
        programChange,   // a = program
        channelPressure, // a = pressure
        polyPressure,    // a = pitch, b = pressure
        sysex            // the payload is sysexBytes [dataStart, dataStart + dataLength)
    };

    struct Event
    {
        // 0 <= tick < loopLength.
        std::int64_t tick = 0;
        EventKind kind = EventKind::cc;
        // What a and b mean depends on the kind; one the kind does not use is 0.
        int a = 0;
        int b = 0;
        // Where a sysex event's payload stands in sysexBytes; 0 and 0 for
        // every other kind.
        std::size_t dataStart = 0;
        std::size_t dataLength = 0;
    };

    // The length of one pass of the loop in ticks, at least 1.
    std::int64_t loopLength = 0;

    std::vector<Note> notes;
    std::vector<Event> events;

    // The payloads of the sysex events one after the other, each without F0
    // and F7.
    std::vector<std::uint8_t> sysexBytes;
};

} // namespace render
