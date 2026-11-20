#pragma once

#include "InputRecorder.h"
#include "ReferenceFile.h"
#include "Statistics.h"

#include <array>
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
// drift, drops, extra messages and a wrong order within one tick are not.
// Absolute lateness of the send path was measured in send mode at milestone 0
// (docs/TIMING.md).
//
// Matching is by message content, each kind in order, within a tolerance
// around the aligned time, so it works across several inputs. Note-offs match
// by channel and pitch (a note-on with velocity 0 counts as one; release
// velocity is not compared).
//
// Order: the messages the reference holds at one tick must arrive in the
// reference's order (docs/RENDERING.md -> Equal-tick order: a retrigger's
// note-off before its note-on, bank select before program change). Matching
// alone cannot see that, so it is checked afterwards, per tick. Only messages
// that arrived on the same input are compared: which of two cables delivered
// first says nothing about the order the app sent in. A tick with any pair
// the wrong way round counts once, and one such tick fails the run. Messages
// of different ticks are not compared; arriving in the wrong order across
// ticks takes lateness of a whole step and shows as that.
//
// Wire time (--din): how long a message takes on the DIN line depends on two
// things the listener cannot see. An interface may or may not use running
// status (leave out a status byte equal to the previous one), and the app may
// or may not send MIDI clock, whose bytes the recorder drops. So net lateness
// is computed under two models, the shortest wire time (running status
// wherever possible, no clock) and the longest (every status byte sent, a
// clock byte 24 times per quarter note, ahead of the messages of its tick).
// Both are reported and judged. Where the two verdicts differ the run is
// INCONCLUSIVE: the figures cannot say whether the targets were met.
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
    // Met under every wire-time model. False also for an inconclusive run.
    bool targetsMet() const { return met; }
    // --din only: met under one wire-time model and not under the other.
    bool inconclusive() const { return undecided; }
    // One row per reference message: times relative to the alignment, lateness, wire time.
    std::string csv() const;

private:
    struct Outcome
    {
        bool received = false;
        std::int64_t receivedNanoseconds = 0;
        std::array<std::int64_t, 2> wireNanoseconds {}; // per wire-time model
        std::size_t arrival = 0;     // index into Input::received
    };

    // The two wire-time models, as indices. Without --din there is no wire
    // and only the first is used.
    static constexpr std::size_t shortestWire = 0, longestWire = 1;

    // The figures under one wire-time model.
    struct Judgement
    {
        std::int64_t alignment = 0;  // arrival time of reference time 0
        Stats raw, net;
        double drift = 0;            // of the judged lateness, ms per minute
        bool met = false;
    };

    void computeWireTimes();
    std::int64_t coarseAlignment() const;
    void match();
    void checkOrderWithinTicks();
    Judgement judge (std::size_t model) const;
    void summarise();

    Input in;
    std::vector<Outcome> outcomes;   // parallel to the reference messages
    std::int64_t coarse = 0;         // arrival time of reference time 0, to within the match tolerance
    std::array<Judgement, 2> judgements;
    int unexpected = 0, afterEnd = 0;
    int wrongOrderTicks = 0;
    std::string firstWrongOrder;     // the first such tick, for the report
    std::string report;
    bool met = false, undecided = false;
};

} // namespace miditiming
