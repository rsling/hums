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
    // there is no such port or it can't be opened.
    virtual PortHandle open (const std::string& deviceIdentifier) = 0;

    // Closes the port; messages scheduled on it and not yet sent are discarded.
    virtual void close (PortHandle port) = 0;

    // Schedules one complete MIDI 1.0 message (status byte first, SysEx with
    // F0 and F7) to leave at timeNanoseconds. A time in the past means "now".
    // Messages to one port at equal times leave in call order. Returns false
    // if the message was not accepted (malformed, unsupported or no room);
    // the caller counts that as a drop.
    virtual bool send (PortHandle port,
                       std::span<const std::uint8_t> message,
                       std::int64_t timeNanoseconds) noexcept = 0;
};

} // namespace platform
