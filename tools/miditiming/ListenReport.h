#pragma once

#include "InputRecorder.h"
#include "ReferenceFile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace miditiming
{

// Listen mode (milestone 3): compares what arrived from another app against
// that app's SMF export of the same project.
//
// The listener cannot know when the other app meant to start, so lateness is
// relative: arrival times are aligned to the reference so that the
// best-timed message has (net) lateness 0, and every other message's lateness
// is measured from there. A constant delay is invisible this way; jitter,
// drift, drops, extra messages and wrong order are not. Absolute lateness of
// the send path was measured in send mode at milestone 0 (docs/TIMING.md).
//
// Matching is by message content, each kind in order, within a tolerance
// around the aligned time, so it works across several inputs and does not
// depend on equal-time messages arriving in file order. Note-offs match by
// channel and pitch (a note-on with velocity 0 counts as one; release
// velocity is not compared).
class ListenReport
{
public:
    struct Input
    {
        std::vector<std::string> header;
        const ReferenceFile* reference = nullptr;
        std::vector<ReceivedMessage> received; // all inputs, in arrival order
        int inputOverflows = 0;
        bool din = false;                      // one input, looped over a DIN cable
    };

    explicit ListenReport (Input input);

    std::string text() const { return report; }
    bool targetsMet() const { return met; }
    // One row per reference message: times relative to the alignment, lateness, wire time.
    std::string csv() const;

private:
    struct Outcome
    {
        bool received = false;
        std::int64_t receivedNanoseconds = 0;
        std::int64_t wireNanoseconds = 0;
    };

    void computeWireTimes();
    void match();
    void summarise();

    Input in;
    std::vector<Outcome> outcomes;   // parallel to the reference messages
    std::int64_t alignment = 0;      // arrival time of reference time 0
    int unexpected = 0, afterEnd = 0;
    std::string report;
    bool met = false;
};

} // namespace miditiming
