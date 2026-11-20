#pragma once

#include "platform/MidiScheduler.h"
#include "platform/mac/MachTime.h"

#include <CoreMIDI/CoreMIDI.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace platform
{

// MidiScheduler that hands CoreMIDI packets with future host-time timestamps,
// so the driver, not our thread, decides when the bytes leave. The engine's
// send path, chosen at milestone 0 (docs/TIMING.md).
class CoreMidiScheduler final : public MidiScheduler
{
public:
    static constexpr int maxPorts = 64;

    CoreMidiScheduler();
    ~CoreMidiScheduler() override;

    CoreMidiScheduler (const CoreMidiScheduler&) = delete;
    CoreMidiScheduler& operator= (const CoreMidiScheduler&) = delete;

    // False if the CoreMIDI client or output port couldn't be created.
    bool isReady() const noexcept { return outputPort != 0; }

    // deviceIdentifier is juce::MidiDeviceInfo::identifier for an output.
    // A destination that is already open is refused.
    PortHandle open (const std::string& deviceIdentifier) override;

    void close (PortHandle port) override;

    // False once the destination has gone (see gone below). True means
    // handed to CoreMIDI for a destination it still lists, not delivered.
    bool send (PortHandle port,
               std::span<const std::uint8_t> message,
               std::int64_t timeNanoseconds) noexcept override;

private:
    void discardPending (std::size_t slot);
    void watchPorts();
    void markGonePorts();

    MachTime machTime;
    MIDIClientRef client = 0;
    MIDIPortRef outputPort = 0;

    // The open ports. send() only reads the atomics. open(), close() and the
    // watcher thread take slotsChanging, so the watcher can never mark a slot
    // that has been closed and given to another destination in the meantime.
    std::mutex slotsChanging;
    std::array<std::atomic<MIDIEndpointRef>, maxPorts> endpoints {};

    // Set by the watcher thread when CoreMIDI no longer lists the port's
    // destination or lists it as offline (interface unplugged or switched
    // off, virtual port removed); cleared when the slot is opened again.
    std::array<std::atomic<bool>, maxPorts> gone {};

    // Per port, the latest host time a message was scheduled for; close()
    // compares it with now to see whether anything is still pending. Written
    // by send() on the engine thread.
    std::array<std::atomic<std::uint64_t>, maxPorts> latestScheduled {};

    // The watcher: a plain thread that looks at the open ports every
    // watchPeriod. CoreMIDI's own notifications are not used for this: on
    // macOS 27 they arrive only while the run loop of the thread that created
    // the process's first MIDI client is running (checked 20 Nov 2026, also
    // for MIDIClientCreateWithBlock), which a command-line tool does not have
    // and which this class cannot arrange.
    std::thread watcher;
    std::mutex watcherWaiting;
    std::condition_variable watcherWake;
    bool watcherShouldStop = false; // guarded by watcherWaiting

    // Scratch space, allocated once so send() never allocates: the UMP words
    // of one message (room for the largest SysEx) and one event list of the
    // largest size CoreMIDI takes. Only the engine thread uses them.
    std::vector<std::uint32_t> words;
    std::vector<std::uint64_t> eventListStorage;
};

} // namespace platform
