#include "ListenReport.h"

#include "Statistics.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace miditiming
{

namespace
{
    // A message more than this far from its aligned time is not its match.
    // Generous against jitter, small against a sixteenth at any sane tempo.
    constexpr std::int64_t matchTolerance = 50'000'000;

    // How many of the first arrivals have a say in the coarse alignment.
    constexpr std::size_t alignmentVoters = 32;

    // What two messages must share to match. Note-offs: channel and pitch.
    std::uint32_t keyOf (const std::array<std::uint8_t, 3>& bytes, std::uint8_t size)
    {
        auto status = bytes[0];
        auto data2 = size > 2 ? bytes[2] : std::uint8_t (0);
        const auto kind = status & 0xf0;

        if (kind == 0x80 || (kind == 0x90 && data2 == 0))
        {
            status = static_cast<std::uint8_t> (0x80 | (status & 0x0f));
            data2 = 0;
        }

        return (std::uint32_t (status) << 16) | (std::uint32_t (size > 1 ? bytes[1] : 0) << 8) | data2;
    }
} // namespace

ListenReport::ListenReport (Input input)
    : in (std::move (input)),
      outcomes (in.reference->messages().size())
{
    computeWireTimes();
    match();
    checkOrderWithinTicks();
    summarise();
}

// The DIN line as a queue over the reference order, as in send mode, under
// the two wire-time models (ListenReport.h). Each message's wire time is the
// moment its last byte has crossed, measured from the message's own time.
void ListenReport::computeWireTimes()
{
    if (! in.din)
        return;

    const auto& messages = in.reference->messages();

    // Shortest: running status wherever two consecutive messages share a
    // status byte, and nothing else on the line.
    {
        std::int64_t lineFree = std::numeric_limits<std::int64_t>::min();
        std::uint8_t lastStatus = 0;

        for (std::size_t i = 0; i < messages.size(); ++i)
        {
            const auto& m = messages[i];
            const auto bytes = m.size - (m.bytes[0] == lastStatus ? 1 : 0);
            const auto begins = std::max (m.offsetNanoseconds, lineFree);
            lineFree = begins + bytes * dinNanosecondsPerByte;
            lastStatus = m.bytes[0];
            outcomes[i].wireNanoseconds[shortestWire] = lineFree - m.offsetNanoseconds;
        }
    }

    // Longest: every status byte, and one clock byte per pulse, 24 pulses per
    // quarter note from tick 0. A pulse on a message's tick goes ahead of it.
    {
        std::int64_t lineFree = std::numeric_limits<std::int64_t>::min();
        const std::int64_t ticksPerQuarter = in.reference->ticksPerQuarter();
        std::int64_t pulse = 0;
        const auto pulseTick = [&] { return pulse * ticksPerQuarter / 24; };

        for (std::size_t i = 0; i < messages.size(); ++i)
        {
            const auto& m = messages[i];

            for (; pulseTick() <= m.tick; ++pulse)
                lineFree = std::max (in.reference->nanosecondsAt (pulseTick()), lineFree) + dinNanosecondsPerByte;

            const auto begins = std::max (m.offsetNanoseconds, lineFree);
            lineFree = begins + m.size * dinNanosecondsPerByte;
            outcomes[i].wireNanoseconds[longestWire] = lineFree - m.offsetNanoseconds;
        }
    }
}

// Where reference time 0 lies on the arrival clock, to well within
// matchTolerance. Each of the first arrivals proposes "I am the first
// reference message of my kind", and the proposal that the most kinds of
// message agree with wins, the earliest on a tie. So a stray message that
// arrives before playback starts cannot misplace everything, as it would if
// the first arrival alone were trusted.
std::int64_t ListenReport::coarseAlignment() const
{
    const auto& messages = in.reference->messages();
    const auto& got = in.received;

    struct Proposal
    {
        std::int64_t alignment = 0;
        std::uint32_t key = 0;
    };

    std::vector<Proposal> proposals;
    const auto voters = std::min (got.size(), alignmentVoters);

    for (std::size_t i = 0; i < voters; ++i)
    {
        const auto key = keyOf (got[i].bytes, got[i].size);
        const auto first = std::find_if (messages.begin(), messages.end(),
                                         [&] (const ReferenceMessage& m) { return keyOf (m.bytes, m.size) == key; });

        if (first != messages.end())
            proposals.push_back ({ got[i].timeNanoseconds - first->offsetNanoseconds, key });
    }

    // If nothing recognisable arrived, pair the first arrival with the first
    // reference message.
    std::int64_t best = got.front().timeNanoseconds - messages.front().offsetNanoseconds;
    std::size_t mostVotes = 0;

    for (const auto& candidate : proposals)
    {
        // One vote per kind of message, so that a message repeated in quick
        // succession cannot outvote the others.
        std::set<std::uint32_t> agreeing;

        for (const auto& other : proposals)
            if (other.alignment >= candidate.alignment - matchTolerance && other.alignment <= candidate.alignment + matchTolerance)
                agreeing.insert (other.key);

        if (agreeing.size() > mostVotes)
        {
            mostVotes = agreeing.size();
            best = candidate.alignment;
        }
    }

    return best;
}

void ListenReport::match()
{
    const auto& messages = in.reference->messages();
    const auto& got = in.received;
    if (got.empty())
        return;

    coarse = coarseAlignment();

    std::map<std::uint32_t, std::vector<std::size_t>> expectedByKey, gotByKey;
    for (std::size_t i = 0; i < messages.size(); ++i)
        expectedByKey[keyOf (messages[i].bytes, messages[i].size)].push_back (i);
    for (std::size_t i = 0; i < got.size(); ++i)
        gotByKey[keyOf (got[i].bytes, got[i].size)].push_back (i);

    const auto lastExpected = coarse + messages.back().offsetNanoseconds + matchTolerance;
    std::vector<bool> used (got.size(), false);

    // Per kind, walk both lists in time order: an arrival far ahead of the
    // next expected message means that one was dropped; one far behind
    // was not expected at all.
    for (const auto& [key, expected] : expectedByKey)
    {
        const auto found = gotByKey.find (key);
        if (found == gotByKey.end())
            continue;

        const auto& arrivals = found->second;
        std::size_t e = 0, a = 0;
        while (e < expected.size() && a < arrivals.size())
        {
            const auto aligned = coarse + messages[expected[e]].offsetNanoseconds;
            const auto at = got[arrivals[a]].timeNanoseconds;

            if (at < aligned - matchTolerance) { ++a; continue; }   // not expected
            if (at > aligned + matchTolerance) { ++e; continue; }   // expected one dropped

            outcomes[expected[e]].received = true;
            outcomes[expected[e]].receivedNanoseconds = at;
            outcomes[expected[e]].arrival = arrivals[a];
            used[arrivals[a]] = true;
            ++e;
            ++a;
        }
    }

    for (std::size_t i = 0; i < got.size(); ++i)
        if (! used[i])
            (got[i].timeNanoseconds > lastExpected ? afterEnd : unexpected)++;
}

// For every tick of the reference: did its messages arrive in the reference's
// order? Input::received is in arrival order, so for two messages of one
// input the smaller index arrived first. Two identical messages on one tick
// can never be told apart and so never count as swapped.
void ListenReport::checkOrderWithinTicks()
{
    const auto& messages = in.reference->messages();
    const auto& got = in.received;

    const auto hex = [] (const auto& m)
    {
        char text[16];
        if (m.size > 2)
            std::snprintf (text, sizeof (text), "%02X %02X %02X", m.bytes[0], m.bytes[1], m.bytes[2]);
        else
            std::snprintf (text, sizeof (text), "%02X %02X", m.bytes[0], m.bytes[1]);
        return std::string (text);
    };

    std::size_t first = 0;
    while (first < messages.size())
    {
        auto last = first;
        while (last < messages.size() && messages[last].tick == messages[first].tick)
            ++last;

        // Each received message of this tick against the latest-arrived one
        // before it in the reference, per input.
        std::map<int, std::size_t> latestOfInput; // input -> reference index
        bool wrong = false;

        for (auto i = first; i < last && ! wrong; ++i)
        {
            if (! outcomes[i].received)
                continue;

            const auto input = got[outcomes[i].arrival].input;
            const auto earlier = latestOfInput.find (input);

            if (earlier == latestOfInput.end() || outcomes[earlier->second].arrival < outcomes[i].arrival)
            {
                latestOfInput[input] = i;
                continue;
            }

            wrong = true;
            if (wrongOrderTicks == 0)
            {
                char where[80];
                std::snprintf (where, sizeof (where), "tick %lld (%.3f s): ", static_cast<long long> (messages[i].tick),
                               static_cast<double> (messages[i].offsetNanoseconds) / 1.0e9);
                firstWrongOrder = where + hex (messages[i]) + " arrived before " + hex (messages[earlier->second])
                                  + ", which the reference has first";
            }
        }

        if (wrong)
            ++wrongOrderTicks;

        first = last;
    }
}

// Lateness under one wire-time model. Fine alignment: the best-timed message
// gets (net) lateness 0, so the alignment itself depends on the model.
ListenReport::Judgement ListenReport::judge (std::size_t model) const
{
    const auto& messages = in.reference->messages();
    Judgement j;

    j.alignment = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 0; i < messages.size(); ++i)
        if (outcomes[i].received)
            j.alignment = std::min (j.alignment, outcomes[i].receivedNanoseconds - messages[i].offsetNanoseconds
                                                     - outcomes[i].wireNanoseconds[model]);

    if (j.alignment == std::numeric_limits<std::int64_t>::max())
        j.alignment = coarse;

    std::vector<std::int64_t> raw, net;
    std::vector<std::pair<double, double>> judgedOverTime; // (minutes, ms)
    int dropped = 0;

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        if (! outcomes[i].received)
        {
            ++dropped;
            continue;
        }

        const auto lateness = outcomes[i].receivedNanoseconds - j.alignment - messages[i].offsetNanoseconds;
        const auto judged = lateness - outcomes[i].wireNanoseconds[model]; // wire time is 0 without --din
        raw.push_back (lateness);
        net.push_back (judged);
        judgedOverTime.emplace_back (static_cast<double> (messages[i].offsetNanoseconds) / 60.0e9,
                                     static_cast<double> (judged) / 1.0e6);
    }

    j.raw = statsOf (raw);
    j.net = statsOf (net);
    j.drift = slopeOf (judgedOverTime);

    // [D1]: software loopback judged on raw lateness, DIN loops net of wire time.
    const auto& judged = in.din ? j.net : j.raw;
    j.met = judged.count > 0 && judged.p99 <= (in.din ? 2.0 : 1.0) && judged.max <= (in.din ? 3.0 : 2.0)
            && dropped + in.inputOverflows == 0 && unexpected == 0 && wrongOrderTicks == 0;
    return j;
}

void ListenReport::summarise()
{
    const auto& messages = in.reference->messages();

    judgements[shortestWire] = judge (shortestWire);
    judgements[longestWire] = in.din ? judge (longestWire) : judgements[shortestWire];

    const auto& shortest = judgements[shortestWire];
    const auto& longest = judgements[longestWire];
    met = shortest.met && longest.met;
    undecided = shortest.met != longest.met;

    const auto received = shortest.raw.count;
    const auto dropped = messages.size() - received;
    char buffer[200];

    std::ostringstream out;
    out << "== miditiming listen report ==\n";
    for (const auto& h : in.header)
        out << h << "\n";
    out << "\n";

    if (in.din)
    {
        out << statsLine ("lateness, raw (relative)", shortest.raw) << "\n";
        out << statsLine ("net, shortest wire time", shortest.net) << "\n";
        out << statsLine ("net, longest wire time", longest.net) << "\n";
        std::snprintf (buffer, sizeof (buffer), "drift (net): %+.4f ms per minute (shortest), %+.4f (longest)", shortest.drift, longest.drift);
    }
    else
    {
        out << statsLine ("lateness, raw (relative)", shortest.raw) << "\n";
        std::snprintf (buffer, sizeof (buffer), "drift (raw): %+.4f ms per minute", shortest.drift);
    }
    out << buffer << "\n";

    out << "reference messages " << messages.size() << ", received " << received
        << ", dropped " << dropped << ", unexpected " << unexpected
        << ", after the end (ignored) " << afterEnd
        << ", input overflow " << in.inputOverflows
        << ", SysEx in reference (not compared) " << in.reference->sysexSkipped() << "\n";

    out << "ticks whose messages arrived in the wrong order: " << wrongOrderTicks << "\n";
    if (wrongOrderTicks > 0)
        out << "  first: " << firstWrongOrder << "\n";

    const auto verdict = [] (bool isMet) { return isMet ? "MET" : "NOT MET"; };

    if (in.din)
    {
        out << "\ntargets [D1] on net relative lateness: p99 <= 2 ms, max <= 3 ms, zero drops, nothing unexpected, "
               "no tick in the wrong order\n"
            << "  with the shortest wire time -> " << verdict (shortest.met) << "\n"
            << "  with the longest wire time  -> " << verdict (longest.met) << "\n"
            << "  -> " << (undecided ? "INCONCLUSIVE" : verdict (met)) << "\n";
        out << "(relative: aligned so the best-timed message has net lateness 0; a constant delay does not show here)\n";
        out << "(wire time = the message's own bytes plus the bytes queued ahead of it, 0.32 ms per byte.\n"
               " shortest: running status wherever the reference allows it, no MIDI clock on the line.\n"
               " longest: every status byte sent, and a clock byte 24 times per quarter note.\n"
               " the raw line uses the shortest model's alignment)\n";
        if (undecided)
            out << "(INCONCLUSIVE: whether the targets were met depends on what the interface and the app put on the\n"
                   " line, which the listener cannot see)\n";
    }
    else
    {
        out << "\ntargets [D1] on raw relative lateness: p99 <= 1 ms, max <= 2 ms, zero drops, nothing unexpected, "
               "no tick in the wrong order -> " << verdict (met) << "\n";
        out << "(relative: aligned so the best-timed message has lateness 0; a constant delay does not show here)\n";
    }

    report = out.str();
}

std::string ListenReport::csv() const
{
    const auto& messages = in.reference->messages();

    std::ostringstream out;
    out << "index,tick,status,data1,data2,reference_ns,received_ns,lateness_ns,wire_shortest_ns,wire_longest_ns,input,arrival_index\n";

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto& m = messages[i];
        out << i << ',' << m.tick << ',' << int (m.bytes[0]) << ',' << int (m.bytes[1]) << ',' << (m.size > 2 ? int (m.bytes[2]) : 0) << ','
            << m.offsetNanoseconds << ',';

        if (outcomes[i].received)
        {
            // Relative to the shortest-wire model's alignment.
            const auto relative = outcomes[i].receivedNanoseconds - judgements[shortestWire].alignment;
            out << relative << ',' << (relative - m.offsetNanoseconds);
        }
        else
        {
            out << ',';
        }

        out << ',' << outcomes[i].wireNanoseconds[shortestWire] << ',' << outcomes[i].wireNanoseconds[longestWire] << ',';

        // Which input it arrived on and its place among all arrivals: what the
        // order check compares.
        if (outcomes[i].received)
            out << in.received[outcomes[i].arrival].input << ',' << outcomes[i].arrival;
        else
            out << ',';

        out << '\n';
    }

    return out.str();
}

} // namespace miditiming
