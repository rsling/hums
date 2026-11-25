#include "render/Renderer.h"

#include <array>
#include <cassert>
#include <optional>

namespace render
{

namespace
{

    Renderer::EventKind kindOf (Kernel::EventKind kind) noexcept
    {
        switch (kind)
        {
            case Kernel::EventKind::noteOn:
                return Renderer::EventKind::noteOn;
            case Kernel::EventKind::cc:
                return Renderer::EventKind::cc;
            case Kernel::EventKind::pitchBend:
                return Renderer::EventKind::pitchBend;
            case Kernel::EventKind::programChange:
                return Renderer::EventKind::programChange;
            case Kernel::EventKind::channelPressure:
                return Renderer::EventKind::channelPressure;
            case Kernel::EventKind::polyPressure:
                return Renderer::EventKind::polyPressure;
            case Kernel::EventKind::sysex:
                return Renderer::EventKind::sysex;
        }

        assert (false);
        return Renderer::EventKind::cc;
    }

    Renderer::Event eventOf (const NoteTracker::NoteOff& off) noexcept
    {
        Renderer::Event event;
        event.tick = off.tick;
        event.track = off.track;
        event.destination = off.destination;
        event.channel = off.channel;
        event.kind = Renderer::EventKind::noteOff;
        event.a = off.pitch;
        event.b = off.offVelocity;
        return event;
    }

    Renderer::Event eventOf (const Kernel::Event& played, std::size_t destination) noexcept
    {
        Renderer::Event event;
        event.tick = played.tick;
        event.track = played.track;
        event.destination = destination;
        event.channel = played.channel;
        event.kind = kindOf (played.kind);
        event.a = played.a;
        event.b = played.b;
        event.templateIndex = played.templateIndex;
        event.dataStart = played.dataStart;
        event.dataLength = played.dataLength;
        return event;
    }

// The off that is to end a note the Kernel gave.
    NoteTracker::NoteOff offOf (const Kernel::Event& noteOn, std::size_t destination) noexcept
    {
        NoteTracker::NoteOff off;
        off.tick = noteOn.offTick;
        off.destination = destination;
        off.channel = noteOn.channel;
        off.pitch = noteOn.a;
        off.offVelocity = noteOn.offVelocity;
        off.track = noteOn.track;
        return off;
    }

// The caller's buffer while it is filled.
    struct Output
    {
        std::span<Renderer::Event> buffer;
        std::size_t eventCount = 0;

        std::size_t room() const noexcept
        {
            return buffer.size() - eventCount;
        }

        void write (const Renderer::Event& event) noexcept
        {
            assert (room() > 0);
            buffer[eventCount] = event;
            ++eventCount;
        }
    };

// Writes every off that is due at the tick or before it. False when the
// buffer ran full first: the tracker may then still hold such offs.
    bool writeOffsDueBy (NoteTracker& tracker, std::int64_t tick, Output& output) noexcept
    {
        while (output.room() > 0)
        {
            const std::optional<NoteTracker::NoteOff> off = tracker.takeOffDueBy (tick);

            if (! off.has_value())
                return true;

            output.write (eventOf (*off));
        }

        // No room is left, and whether an off is: the tracker cannot be asked
        // without taking one. The caller comes back and finds out.
        return false;
    }

// How many events the Kernel is asked for at a time. They stand on the
// stack, so that nothing is allocated; the number only says how often the
// Kernel is asked, not what is played.
    constexpr std::size_t kernelEventsAtATime = 64;

} // namespace

Renderer::Progress Renderer::render (const Snapshot& snapshot,
                                     const std::vector<bool>& audibleTracks,
                                     std::span<const std::size_t> destinationOfTrack,
                                     NoteTracker& tracker,
                                     Kernel::Position from,
                                     std::int64_t end,
                                     std::span<Event> buffer) noexcept
{
    assert (destinationOfTrack.size() == snapshot.trackPlans.size());
    assert (from.tick >= 0 && from.tick <= end);
    assert (buffer.size() >= 2);

    Output output;
    output.buffer = buffer;

    std::array<Kernel::Event, kernelEventsAtATime> kernelEvents;
    Kernel::Position position = from;

    while (position.tick < end)
    {
        const Kernel::Progress kernelProgress = Kernel::expand (snapshot, audibleTracks, position, end, kernelEvents);

        for (std::size_t index = 0; index < kernelProgress.eventCount; ++index)
        {
            const Kernel::Event& played = kernelEvents[index];

            // The offs that are due come before anything else at a tick. Then
            // there must be room for the event, and for a note-on also for
            // the off of an earlier note of its pitch. If there is not, the
            // caller comes back to this very event.
            if (! writeOffsDueBy (tracker, played.tick, output) || output.room() < 2)
                return { output.eventCount, position, false };

            const std::size_t destination = destinationOfTrack[played.track];

            if (played.kind == Kernel::EventKind::noteOn)
            {
                const std::optional<NoteTracker::NoteOff> offOfEarlierNote = tracker.noteOn (played.tick, offOf (played, destination));

                if (offOfEarlierNote.has_value())
                    output.write (eventOf (*offOfEarlierNote));
            }

            output.write (eventOf (played, destination));

            // The Kernel counts the events it has given at a tick.
            position = played.tick == position.tick ? Kernel::Position { position.tick, position.eventsTaken + 1 }
                                                    : Kernel::Position { played.tick, 1 };
        }

        // When the Kernel's own buffer was full, it goes on behind the last
        // event, which is where `position` is; otherwise it is at the end.
        if (kernelProgress.next.tick == end)
            position = kernelProgress.next;
    }

    // The offs of the ticks behind the last event. One that falls on `end`
    // itself belongs to the next window.
    if (end > 0 && ! writeOffsDueBy (tracker, end - 1, output))
        return { output.eventCount, position, false };

    return { output.eventCount, position, true };
}

std::size_t Renderer::stop (NoteTracker& tracker, std::int64_t stopTick, std::span<Event> buffer) noexcept
{
    assert (! buffer.empty());

    Output output;
    output.buffer = buffer;

    while (output.room() > 0)
    {
        const std::optional<NoteTracker::NoteOff> off = tracker.takeOffAtStop (stopTick);

        if (! off.has_value())
            break;

        output.write (eventOf (*off));
    }

    return output.eventCount;
}

} // namespace render
