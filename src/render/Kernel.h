#pragma once

#include "render/snapshot/Snapshot.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace render
{

// Expands a Snapshot over a window of ticks into the events that the
// arrangement plays there (docs/RENDERING.md -> Per track, Note windows).
// The engine runs it per look-ahead window, SMF export over the export range:
// one implementation.
//
// For every placement of every audible track, the clip loops inside
// [start, start + length), beginning `offset` ticks into its loop:
//
// - a note or an event is played wherever its tick comes round inside the
//   placement; what stands before `offset` is first played in the next pass
//   of the loop, and a note that began before the placement did is not
//   brought in late;
// - a note plays its full length, also across the loop point into the next
//   pass, but not past the end of its placement: there it is cut.
//
// What comes out is in the order of the ticks. At one tick the tracks come in
// their own order, and for each track first its events that are no notes,
// then its notes, each group in the order the template has them.
//
// The Kernel sends no note-off. A note comes out once, as a note-on that
// knows the tick of its off; render::Renderer hands it to the NoteTracker,
// which holds the off until it is due.
//
// The Kernel keeps nothing between two calls: what a window holds follows
// from the Snapshot alone, so any stretch of ticks can be asked for at any
// time, in one piece or in several, and gives the same events. Std only, and
// it allocates nothing.
class Kernel
{
public:
    enum class EventKind
    {
        noteOn,          // a = pitch, b = velocity
        cc,              // a = controller, b = value
        pitchBend,       // a = value 0-16383, 8192 = centre
        programChange,   // a = program
        channelPressure, // a = pressure
        polyPressure,    // a = pitch, b = pressure
        sysex            // the payload is in the template the event came from
    };

    struct Event
    {
        std::int64_t tick = 0;
        // The Track that plays the event, by its position among the tracks.
        std::size_t track = 0;
        // 1 to 16: the track's channel.
        int channel = 1;
        EventKind kind = EventKind::cc;
        // What a and b mean depends on the kind; one the kind does not use is 0.
        int a = 0;
        int b = 0;

        // A note-on only: where its note-off falls, later than `tick` and at
        // the latest at the end of the placement, and the off's velocity.
        std::int64_t offTick = 0;
        int offVelocity = 0;

        // A sysex event only: its payload is the bytes [dataStart, dataStart
        // + dataLength) of sysexBytes in the Snapshot's clipTemplates
        // [templateIndex].
        std::size_t templateIndex = 0;
        std::size_t dataStart = 0;
        std::size_t dataLength = 0;
    };

    // How far an expansion has come: every event before `tick` has been
    // given, and of the events at `tick` itself the first `eventsTaken`. A
    // window is begun at { its first tick, 0 }.
    struct Position
    {
        std::int64_t tick = 0;
        std::size_t eventsTaken = 0;
    };

    struct Progress
    {
        // How many events were written to the front of the buffer.
        std::size_t eventCount = 0;
        // Where to go on. Its tick is the end of the window when the window
        // is done.
        Position next;
    };

    // Writes the events from `from` up to the tick `end`, which itself is not
    // played, into the buffer, and stops early when the buffer is full. The
    // caller goes on from the returned position, with the same Snapshot and
    // the same audible tracks, until that position's tick is `end`:
    //
    //     for (Kernel::Position position { start, 0 }; position.tick < end;)
    //     {
    //         const auto progress = Kernel::expand (snapshot, audibleTracks, position, end, buffer);
    //         ... the first progress.eventCount events of the buffer ...
    //         position = progress.next;
    //     }
    //
    // However many events share a tick, a buffer of any size gets through
    // them; it must have room for one at least.
    //
    // audibleTracks says for each Track, in track order, whether it is heard:
    // the result of mute and solo, which the Snapshot does not hold. A track
    // that is not heard gives no events at all.
    //
    // 0 <= from.tick <= end.
    static Progress expand (const Snapshot& snapshot,
                            const std::vector<bool>& audibleTracks,
                            Position from,
                            std::int64_t end,
                            std::span<Event> buffer) noexcept;
};

} // namespace render
