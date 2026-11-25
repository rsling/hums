#pragma once

#include "render/Kernel.h"
#include "render/NoteTracker.h"
#include "render/snapshot/Snapshot.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace render
{

// Kernel + NoteTracker: the events of a window of ticks exactly as they go
// out, note-offs included (docs/RENDERING.md -> Pipeline). It is the one
// place where the Kernel's output is put through the NoteTracker, so that the
// engine, SMF export and the golden tests cannot differ in it.
//
// At one tick the order is that of docs/RENDERING.md -> Equal-tick order:
// first the note-offs that are due, then what the Kernel gives for that tick,
// in the Kernel's order. A note-on of a pitch that still sounds on its
// destination is preceded by the earlier note's off (Same-pitch overlap).
//
// The Renderer keeps nothing itself. The notes that sound are held by the
// NoteTracker the caller hands in, from one window to the next. Std only, and
// it allocates nothing.
class Renderer
{
public:
    enum class EventKind
    {
        noteOff,         // a = pitch, b = off velocity
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
        // Where the event goes: the track's destination, as the caller
        // numbers them.
        std::size_t destination = 0;
        // 1 to 16.
        int channel = 1;
        EventKind kind = EventKind::cc;
        // What a and b mean depends on the kind; one the kind does not use is 0.
        int a = 0;
        int b = 0;

        // A sysex event only: its payload is the bytes [dataStart, dataStart
        // + dataLength) of sysexBytes in the Snapshot's clipTemplates
        // [templateIndex].
        std::size_t templateIndex = 0;
        std::size_t dataStart = 0;
        std::size_t dataLength = 0;
    };

    struct Progress
    {
        // How many events were written to the front of the buffer.
        std::size_t eventCount = 0;
        // Where to go on when the window is not finished.
        Kernel::Position next;
        // Whether everything of the window has been written.
        bool isFinished = false;
    };

    // Writes what is played from `from` up to the tick `end`, which itself is
    // not played, into the buffer: the Kernel's events of those ticks and the
    // note-offs that fall due at them. It stops early when the buffer is
    // full; the caller goes on from the returned position, with the same
    // arguments otherwise, until the window is finished:
    //
    //     Kernel::Position position { start, 0 };
    //
    //     for (bool isFinished = false; ! isFinished;)
    //     {
    //         const auto progress = Renderer::render (snapshot, audibleTracks, destinationOfTrack, tracker, position, end, buffer);
    //         ... the first progress.eventCount events of the buffer ...
    //         position = progress.next;
    //         isFinished = progress.isFinished;
    //     }
    //
    // The next window begins where this one ended, with the same tracker. A
    // note-off that falls on `end` itself belongs to the next window; when
    // there is none because playback ends there, `stop` gives it.
    //
    // audibleTracks is what Kernel::expand takes. destinationOfTrack says for
    // each Track, in track order, into which of the tracker's destinations it
    // plays. The buffer must have room for two events at least: a note-on and
    // the off before it.
    //
    // 0 <= from.tick <= end.
    static Progress render (const Snapshot& snapshot,
                            const std::vector<bool>& audibleTracks,
                            std::span<const std::size_t> destinationOfTrack,
                            NoteTracker& tracker,
                            Kernel::Position from,
                            std::int64_t end,
                            std::span<Event> buffer) noexcept;

    // When playback stops at a tick, or the rendered range ends there: writes
    // the note-off of every note that still sounds into the buffer, each at
    // its own tick or, if it would have come later, at the tick of the stop.
    // Returns how many were written. When the buffer was too small for all of
    // them the tracker still holds some (soundingCount), and the caller calls
    // again:
    //
    //     do
    //     {
    //         const auto offCount = Renderer::stop (tracker, stopTick, buffer);
    //         ... the first offCount events of the buffer ...
    //     } while (tracker.soundingCount() > 0);
    //
    // The buffer must have room for one event at least.
    static std::size_t stop (NoteTracker& tracker, std::int64_t stopTick, std::span<Event> buffer) noexcept;
};

} // namespace render
