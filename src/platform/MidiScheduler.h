#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace platform
{

// Sends MIDI 1.0 messages to output ports at given times on the MonotonicClock.
//
// Threading: open() and close() are message-thread calls. send() is called from
// one thread only (the engine thread) and never allocates, blocks or locks.
// close() must not race a send() to the same handle; the caller stops sending
// to a port before closing it.
class MidiScheduler
{
public:
    using PortHandle = int;
    static constexpr PortHandle invalidPort = -1;

    virtual ~MidiScheduler() = default;

    // Opens the output port the platform reports under deviceIdentifier (the
    // value Routing stores in Player.deviceIdentifier). Returns invalidPort if
    // there is no such port, it can't be opened, or it is already open: a
    // port is open at most once at a time. Whoever sends for several Players
    // bound to one physical port shares one handle among them.
    virtual PortHandle open (const std::string& deviceIdentifier) = 0;

    // Closes the port; messages scheduled on it and not yet sent are discarded.
    virtual void close (PortHandle port) = 0;

    // Schedules one complete MIDI 1.0 message (status byte first, SysEx with
    // F0 and F7) to leave at timeNanoseconds. A time in the past means "now".
    // Messages to one port at equal times leave in call order. Returns false
    // if the message was not accepted: malformed, unsupported, no room, or
    // the port has gone (unplugged or removed) since it was opened. A port
    // that has gone stays refused until it is closed and opened again. The
    // caller counts a refusal as a drop. True means the platform took the
    // message for a port it still knows; no platform confirms delivery, and
    // a message handed over just before the port went may be lost unreported.
    virtual bool send (PortHandle port,
                       std::span<const std::uint8_t> message,
                       std::int64_t timeNanoseconds) noexcept = 0;
};

} // namespace platform
