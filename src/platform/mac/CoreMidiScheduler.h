#pragma once

#include "platform/MidiScheduler.h"
#include "platform/mac/MachTime.h"

#include <CoreMIDI/CoreMIDI.h>

#include <array>
#include <cstdint>
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
    PortHandle open (const std::string& deviceIdentifier) override;
    void close (PortHandle port) override;
    bool send (PortHandle port,
               std::span<const std::uint8_t> message,
               std::int64_t timeNanoseconds) noexcept override;

private:
    MachTime machTime;
    MIDIClientRef client = 0;
    MIDIPortRef outputPort = 0;
    std::array<MIDIEndpointRef, maxPorts> endpoints {};

    // Scratch space for one message, sized once for the largest SysEx, so
    // send() never allocates. Only the engine thread uses them.
    std::vector<std::uint32_t> words;
    std::vector<std::uint64_t> eventListStorage;
};

} // namespace platform
