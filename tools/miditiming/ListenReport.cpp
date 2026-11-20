#include "ListenReport.h"

#include "Statistics.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <map>
#include <sstream>

namespace miditiming
{

namespace
{
    // A message more than this far from its aligned time is not its match.
    // Generous against jitter, small against a sixteenth at any sane tempo.
    constexpr std::int64_t matchTolerance = 50'000'000;

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
    summarise();
}

// The DIN line as a queue over the reference order, as in send mode. Running
// status is assumed wherever it is possible (an interface may use it), which
// gives the shortest wire time and so errs towards overstating net lateness.
void ListenReport::computeWireTimes()
{
    if (! in.din)
        return;

    const auto& messages = in.reference->messages();
    std::int64_t lineFree = std::numeric_limits<std::int64_t>::min();
    std::uint8_t lastStatus = 0;

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto& m = messages[i];
        const auto bytes = m.size - (m.bytes[0] == lastStatus ? 1 : 0);
        const auto begins = std::max (m.offsetNanoseconds, lineFree);
        lineFree = begins + bytes * dinNanosecondsPerByte;
        lastStatus = m.bytes[0];
        outcomes[i].wireNanoseconds = lineFree - m.offsetNanoseconds;
    }
}

void ListenReport::match()
{
    const auto& messages = in.reference->messages();
    const auto& got = in.received;
    if (got.empty())
        return;

    // Coarse alignment: the first arrival is the first reference message of
    // the same kind. Only has to be good to well within matchTolerance.
    const auto firstKey = keyOf (got.front().bytes, got.front().size);
    std::int64_t coarse = got.front().timeNanoseconds - messages.front().offsetNanoseconds;
    for (const auto& m : messages)
        if (keyOf (m.bytes, m.size) == firstKey)
        {
            coarse = got.front().timeNanoseconds - m.offsetNanoseconds;
            break;
        }

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
            used[arrivals[a]] = true;
            ++e;
            ++a;
        }
    }

    for (std::size_t i = 0; i < got.size(); ++i)
        if (! used[i])
            (got[i].timeNanoseconds > lastExpected ? afterEnd : unexpected)++;

    // Fine alignment: the best message gets (net) lateness 0.
    alignment = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 0; i < messages.size(); ++i)
        if (outcomes[i].received)
            alignment = std::min (alignment, outcomes[i].receivedNanoseconds - messages[i].offsetNanoseconds - outcomes[i].wireNanoseconds);

    if (alignment == std::numeric_limits<std::int64_t>::max())
        alignment = coarse;
}

void ListenReport::summarise()
{
    const auto& messages = in.reference->messages();

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

        const auto lateness = outcomes[i].receivedNanoseconds - alignment - messages[i].offsetNanoseconds;
        raw.push_back (lateness);
        if (in.din)
            net.push_back (lateness - outcomes[i].wireNanoseconds);

        const auto judged = in.din ? lateness - outcomes[i].wireNanoseconds : lateness;
        judgedOverTime.emplace_back (static_cast<double> (messages[i].offsetNanoseconds) / 60.0e9,
                                     static_cast<double> (judged) / 1.0e6);
    }

    const auto rawStats = statsOf (raw);
    const auto netStats = statsOf (net);
    const auto& judged = in.din ? netStats : rawStats;
    const auto drops = dropped + in.inputOverflows;

    // [D1]: software loopback judged on raw lateness, DIN loops net of wire time.
    const double p99Target = in.din ? 2.0 : 1.0;
    const double maxTarget = in.din ? 3.0 : 2.0;
    met = judged.count > 0 && judged.p99 <= p99Target && judged.max <= maxTarget && drops == 0 && unexpected == 0;

    std::ostringstream out;
    out << "== miditiming listen report ==\n";
    for (const auto& h : in.header)
        out << h << "\n";
    out << "\n";
    out << statsLine ("lateness, raw (relative)", rawStats) << "\n";
    if (in.din)
        out << statsLine ("lateness, net of DIN wire time", netStats) << "\n";

    char buffer[200];
    std::snprintf (buffer, sizeof (buffer), "drift (%s): %+.4f ms per minute", in.din ? "net" : "raw", slopeOf (judgedOverTime));
    out << buffer << "\n";

    out << "reference messages " << messages.size() << ", received " << raw.size()
        << ", dropped " << dropped << ", unexpected " << unexpected
        << ", after the end (ignored) " << afterEnd
        << ", input overflow " << in.inputOverflows
        << ", SysEx in reference (not compared) " << in.reference->sysexSkipped() << "\n";

    out << "\ntargets [D1] on " << (in.din ? "net" : "raw") << " relative lateness: p99 <= " << p99Target
        << " ms, max <= " << maxTarget << " ms, zero drops, nothing unexpected -> " << (met ? "MET" : "NOT MET") << "\n";
    out << "(relative: aligned so the best-timed message has lateness 0; a constant delay does not show here)\n";
    if (in.din)
        out << "(wire time = the message's own bytes plus the bytes queued ahead of it, running status assumed, 0.32 ms per byte)\n";

    report = out.str();
}

std::string ListenReport::csv() const
{
    const auto& messages = in.reference->messages();

    std::ostringstream out;
    out << "index,status,data1,data2,reference_ns,received_ns,lateness_ns,wire_ns\n";

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto& m = messages[i];
        out << i << ',' << int (m.bytes[0]) << ',' << int (m.bytes[1]) << ',' << (m.size > 2 ? int (m.bytes[2]) : 0) << ','
            << m.offsetNanoseconds << ',';

        if (outcomes[i].received)
        {
            const auto relative = outcomes[i].receivedNanoseconds - alignment;
            out << relative << ',' << (relative - m.offsetNanoseconds);
        }
        else
        {
            out << ',';
        }

        out << ',' << outcomes[i].wireNanoseconds << '\n';
    }

    return out.str();
}

} // namespace miditiming
