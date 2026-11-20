#pragma once

#include "InputRecorder.h"
#include "Sender.h"
#include "TrafficPattern.h"

#include <cstdint>
#include <string>
#include <vector>

namespace miditiming
{

// Matches what was sent against what arrived and summarises it as
// docs/ARCHITECTURE.md -> Timing asks: wake-up jitter, lateness against the
// intended time (raw, and for DIN also net of computed wire time), drift over
// the run, drops. Median, p99 and max per run.
class TimingReport
{
public:
    struct Input
    {
        std::vector<std::string> header;  // "key: value" lines describing the run
        const TrafficPattern* pattern = nullptr;
        const Sender* sender = nullptr;
        std::int64_t startNanoseconds = 0;
        std::vector<std::vector<ReceivedMessage>> receivedPerPort;
        int inputOverflows = 0;
        bool din = false;                  // the loop runs over a DIN cable
    };

    explicit TimingReport (Input input);

    std::string text() const { return report; }
    bool targetsMet() const { return met; }
    // One row per pattern message: port, times in ns, lateness, wire time.
    std::string csv() const;

private:
    struct Outcome
    {
        bool received = false;
        std::int64_t receivedNanoseconds = 0;
        std::int64_t wireNanoseconds = 0; // DIN only: own bytes plus bytes queued ahead
    };

    void match();
    void summarise();

    Input in;
    std::vector<Outcome> outcomes;    // parallel to the pattern's messages
    int unexpected = 0;
    std::string report;
    bool met = false;
};

} // namespace miditiming
