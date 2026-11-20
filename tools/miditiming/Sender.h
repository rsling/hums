#pragma once

#include "TrafficPattern.h"

#include "platform/MidiScheduler.h"
#include "platform/MonotonicClock.h"
#include "platform/RealtimeThread.h"

#include <cstdint>
#include <vector>

namespace miditiming
{

struct WakeRecord
{
    std::int64_t intendedNanoseconds = 0;
    std::int64_t actualNanoseconds = 0;
};

// Plays the pattern the way the engine will: a real-time thread wakes once
// per window and hands the scheduler every message due before now + look-ahead.
// Everything it records goes into storage sized before the run starts.
class Sender
{
public:
    struct Settings
    {
        std::int64_t windowNanoseconds = 2'000'000;
        std::int64_t lookAheadNanoseconds = 10'000'000;
    };

    Sender (platform::MidiScheduler& scheduler,
            const platform::MonotonicClock& clock,
            platform::RealtimeThread& realtime,
            const TrafficPattern& pattern,
            std::vector<platform::MidiScheduler::PortHandle> ports,
            Settings settings);

    // Blocks until the last message has been handed over. Run it on its own
    // thread; startNanoseconds is the pattern's time zero.
    void run (std::int64_t startNanoseconds);

    bool realtimeGranted() const { return granted; }
    int failedSends() const { return failed; }
    const std::vector<WakeRecord>& wakes() const { return wakeLog; }
    // When send() was called for each pattern message, in pattern order.
    const std::vector<std::int64_t>& sendCallTimes() const { return sendLog; }

private:
    platform::MidiScheduler& scheduler;
    const platform::MonotonicClock& clock;
    platform::RealtimeThread& realtime;
    const TrafficPattern& pattern;
    std::vector<platform::MidiScheduler::PortHandle> ports;
    Settings settings;

    bool granted = false;
    int failed = 0;
    std::vector<WakeRecord> wakeLog;
    std::vector<std::int64_t> sendLog;
};

} // namespace miditiming
