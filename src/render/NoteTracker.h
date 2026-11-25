#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace render
{

// The notes that sound, each with the note-off that will end it, kept per
// destination (docs/RENDERING.md -> Same-pitch overlap). It stands between
// the Kernel's output and the MIDI output or the exported file, and makes
// three things sure:
//
// - every note-on gets its note-off, also when playback stops or the Snapshot
//   that produced the note is gone: the off is held here, not in a Snapshot;
// - a destination never holds two notes of one pitch on one channel: a new
//   note-on of a pitch that still sounds is preceded by the earlier note's
//   off, and the off that note would have had later is never sent;
// - the offs that fall due come out in the order of their ticks.
//
// A destination is whatever receives the notes as one stream; the tracker
// only numbers them, from 0. The engine numbers its physical ports. Without
// hardware (SMF export, the golden tests) a Player is one destination, bound
// or not, and every track without a Player is one of its own (decided 25 Nov
// 2026). Two tracks on one destination and one channel share their pitches;
// the same channel on two destinations shares nothing.
//
// The tracker sends nothing itself. Whoever drives it asks for the offs and
// puts them into its output, at one tick in this order: first every off that
// is due (takeOffDueBy, until it gives none), then the events that are no
// notes, then the note-ons, each through noteOn.
//
// Std only, like the rest of the render core. It allocates when it is made
// and never afterwards, and it cannot run full: there is room for one
// sounding note per pitch, channel and destination, and there is never a
// second.
class NoteTracker
{
public:
    static constexpr int channelCount = 16;
    static constexpr int pitchCount = 128;

    // A note-off and where it goes.
    struct NoteOff
    {
        std::int64_t tick = 0;
        // 0 to destinationCount - 1.
        std::size_t destination = 0;
        // 1 to 16.
        int channel = 1;
        // 0 to 127.
        int pitch = 0;
        // 0 to 127.
        int offVelocity = 0;
        // The Track whose note this off ends, by its position among the
        // tracks. The tracker only carries it along: an export writes the off
        // into that track's part of the file.
        std::size_t track = 0;
    };

    // Room for the notes of this many destinations.
    explicit NoteTracker (std::size_t destinationCount);

    // A note-on at onTick, together with the off that is to end it: off.tick
    // is later than onTick. From now on the tracker holds that off.
    //
    // If a note of the same destination, channel and pitch still sounds, its
    // off is returned and must be sent immediately before the new note-on. It
    // is returned at onTick, or at its own tick if that is earlier, and the
    // tracker forgets it: it is not sent a second time. This holds also for a
    // note that began at onTick itself: two notes of one pitch at one tick
    // are sent as on, off, on (decided 25 Nov 2026).
    std::optional<NoteOff> noteOn (std::int64_t onTick, const NoteOff& off) noexcept;

    // The next off that is due at the tick or before it, and none when no
    // such off is held. Offs come in the order of their ticks, and those of
    // one tick in the order in which their notes began. The tracker forgets
    // what it returns.
    std::optional<NoteOff> takeOffDueBy (std::int64_t tick) noexcept;

    // When playback stops at a tick, or leaves it for another place (seek,
    // the wrap of the transport loop): the next off held, whenever it would
    // have been due, and none when nothing sounds any more. An off that would
    // have been due later is returned at the tick of the stop. The order is
    // that of takeOffDueBy.
    std::optional<NoteOff> takeOffAtStop (std::int64_t stopTick) noexcept;

    // How many notes sound, that is, how many offs are held.
    std::size_t soundingCount() const noexcept;

private:
    // Takes the off at a position out of the list and returns it, its tick
    // brought forward to latestTick if it was due later.
    NoteOff takeAt (std::size_t position, std::int64_t latestTick) noexcept;

    std::size_t destinationCount = 0;

    // The offs held, in the order in which they fall due. A list in order
    // and not a table by pitch: few notes sound at a time, and the next off
    // due is asked for far more often than a note begins.
    std::vector<NoteOff> offsInDueOrder;
};

} // namespace render
