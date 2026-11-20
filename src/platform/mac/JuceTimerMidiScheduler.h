#pragma once

#include "platform/MidiScheduler.h"
#include "platform/MonotonicClock.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <memory>

namespace platform
{

// MidiScheduler on JUCE alone: send() queues the message, and a 1 ms
// juce::HighResolutionTimer sends every message that has fallen due with
// juce::MidiOutput::sendMessageNow. The first timing candidate in
// docs/ARCHITECTURE.md -> Timing. Messages of up to three bytes only; SysEx is
// refused (send returns false) until milestone 0 picks this backend, if it does.
class JuceTimerMidiScheduler final : public MidiScheduler,
                                     private juce::HighResolutionTimer
{
public:
    static constexpr int maxPorts = 64;
    static constexpr int queueCapacity = 8192;

    explicit JuceTimerMidiScheduler (const MonotonicClock& clock);
    ~JuceTimerMidiScheduler() override;

    // deviceIdentifier is juce::MidiDeviceInfo::identifier for an output.
    PortHandle open (const std::string& deviceIdentifier) override;
    void close (PortHandle port) override;
    bool send (PortHandle port,
               std::span<const std::uint8_t> message,
               std::int64_t timeNanoseconds) noexcept override;

    // Messages the timer could not keep because its pending list was full.
    int overflowCount() const noexcept { return overflows.load(); }

private:
    struct Pending
    {
        std::int64_t timeNanoseconds = 0;
        PortHandle port = invalidPort;
        std::array<std::uint8_t, 3> bytes {};
        std::uint8_t size = 0;
    };

    void hiResTimerCallback() override;
    void takeQueuedMessages() noexcept;

    const MonotonicClock& clock;
    std::array<std::unique_ptr<juce::MidiOutput>, maxPorts> outputs;

    // send() -> timer thread, single producer, single consumer.
    juce::AbstractFifo queue { queueCapacity };
    std::array<Pending, queueCapacity> queueStorage {};

    // Timer thread only: queued messages not yet due, sorted by time, stable.
    std::array<Pending, queueCapacity> pending {};
    int pendingCount = 0;

    std::atomic<int> overflows { 0 };
};

} // namespace platform
