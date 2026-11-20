#include "TimingReport.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace miditiming
{

namespace
{
    // DIN MIDI: 31 250 baud, 10 bits per byte.
    constexpr std::int64_t dinNanosecondsPerByte = 320'000;

    // How far ahead to look for the received message after a mismatch.
    constexpr std::size_t resyncWindow = 64;

    struct Stats
    {
        std::size_t count = 0;
        double median = 0, p99 = 0, max = 0, min = 0;
    };

    // Nearest-rank percentiles, in milliseconds.
    Stats statsOf (std::vector<std::int64_t> values)
    {
        Stats s;
        s.count = values.size();
        if (values.empty())
            return s;

        std::sort (values.begin(), values.end());
        const auto at = [&] (double p)
        {
            const auto rank = static_cast<std::size_t> (std::ceil (p * static_cast<double> (values.size())));
            return static_cast<double> (values[std::max<std::size_t> (rank, 1) - 1]) / 1.0e6;
        };

        s.median = at (0.5);
        s.p99 = at (0.99);
        s.max = static_cast<double> (values.back()) / 1.0e6;
        s.min = static_cast<double> (values.front()) / 1.0e6;
        return s;
    }

    std::string line (const char* label, const Stats& s)
    {
        char buffer[200];
        std::snprintf (buffer, sizeof (buffer), "%-34s median %8.3f ms   p99 %8.3f ms   max %8.3f ms   (min %.3f, n = %zu)",
                       label, s.median, s.p99, s.max, s.min, s.count);
        return buffer;
    }

    bool sameBytes (const PatternMessage& sent, const ReceivedMessage& got)
    {
        return sent.size == got.size && std::equal (sent.bytes.begin(), sent.bytes.begin() + sent.size, got.bytes.begin());
    }
} // namespace

TimingReport::TimingReport (Input input)
    : in (std::move (input)),
      outcomes (in.pattern->messages().size())
{
    match();
    summarise();
}

// Per port, messages arrive in the order they were sent (one wire, or one IAC
// bus), so matching walks both lists in step. On a mismatch the received
// message is looked for a little further on; what it skips was dropped.
void TimingReport::match()
{
    const auto& messages = in.pattern->messages();

    for (std::size_t port = 0; port < in.receivedPerPort.size(); ++port)
    {
        std::vector<std::size_t> expected;
        for (std::size_t i = 0; i < messages.size(); ++i)
            if (static_cast<std::size_t> (messages[i].port) == port)
                expected.push_back (i);

        // The DIN line as a queue: a message starts when the line is free,
        // never before its intended time.
        std::int64_t lineFree = 0;
        for (auto index : expected)
        {
            const auto intended = in.startNanoseconds + messages[index].offsetNanoseconds;
            const auto begins = std::max (intended, lineFree);
            lineFree = begins + messages[index].size * dinNanosecondsPerByte;
            outcomes[index].wireNanoseconds = in.din ? lineFree - intended : 0;
        }

        const auto& got = in.receivedPerPort[port];
        std::size_t e = 0;

        for (const auto& message : got)
        {
            std::size_t found = e;
            while (found < expected.size() && found < e + resyncWindow && ! sameBytes (messages[expected[found]], message))
                ++found;

            if (found < expected.size() && found < e + resyncWindow)
            {
                outcomes[expected[found]].received = true;
                outcomes[expected[found]].receivedNanoseconds = message.timeNanoseconds;
                e = found + 1;
            }
            else
            {
                ++unexpected;
            }
        }
    }
}

void TimingReport::summarise()
{
    const auto& messages = in.pattern->messages();
    const auto& sendCalls = in.sender->sendCallTimes();

    std::vector<std::int64_t> raw, net, wake, handOver;
    std::vector<std::pair<double, double>> judgedOverTime; // (minutes, ms)
    int dropped = 0, handedLate = 0;

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto intended = in.startNanoseconds + messages[i].offsetNanoseconds;
        handOver.push_back (intended - sendCalls[i]);
        if (sendCalls[i] > intended)
            ++handedLate;

        if (! outcomes[i].received)
        {
            ++dropped;
            continue;
        }

        const auto lateness = outcomes[i].receivedNanoseconds - intended;
        raw.push_back (lateness);
        if (in.din)
            net.push_back (lateness - outcomes[i].wireNanoseconds);

        const auto judged = in.din ? lateness - outcomes[i].wireNanoseconds : lateness;
        judgedOverTime.emplace_back (static_cast<double> (messages[i].offsetNanoseconds) / 60.0e9,
                                     static_cast<double> (judged) / 1.0e6);
    }

    for (const auto& w : in.sender->wakes())
        wake.push_back (w.actualNanoseconds - w.intendedNanoseconds);

    // Drift: least-squares slope of the judged lateness over the run, and the
    // median of the first and last minute.
    double slope = 0;
    if (judgedOverTime.size() > 1)
    {
        double mx = 0, my = 0;
        for (const auto& [x, y] : judgedOverTime) { mx += x; my += y; }
        mx /= static_cast<double> (judgedOverTime.size());
        my /= static_cast<double> (judgedOverTime.size());

        double sxy = 0, sxx = 0;
        for (const auto& [x, y] : judgedOverTime) { sxy += (x - mx) * (y - my); sxx += (x - mx) * (x - mx); }
        slope = sxx > 0 ? sxy / sxx : 0;
    }

    const auto lengthMinutes = static_cast<double> (in.pattern->lengthNanoseconds()) / 60.0e9;
    std::vector<std::int64_t> firstMinute, lastMinute;
    for (const auto& [x, y] : judgedOverTime)
    {
        const auto ns = static_cast<std::int64_t> (y * 1.0e6);
        if (x < 1.0) firstMinute.push_back (ns);
        if (x >= lengthMinutes - 1.0) lastMinute.push_back (ns);
    }

    const auto rawStats = statsOf (raw);
    const auto netStats = statsOf (net);
    const auto& judged = in.din ? netStats : rawStats;
    const auto drops = dropped + in.inputOverflows + in.sender->failedSends();

    // [D1]: software loopback judged on raw lateness, DIN loops net of wire time.
    const double p99Target = in.din ? 2.0 : 1.0;
    const double maxTarget = in.din ? 3.0 : 2.0;
    met = judged.count > 0 && judged.p99 <= p99Target && judged.max <= maxTarget && drops == 0 && unexpected == 0;

    std::ostringstream out;
    out << "== miditiming report ==\n";
    for (const auto& h : in.header)
        out << h << "\n";
    out << "\n";
    out << line ("wake-up lateness (sender thread)", statsOf (wake)) << "\n";
    out << line ("hand-over lead (intended - send())", statsOf (handOver)) << "\n";
    out << line ("lateness, raw", rawStats) << "\n";
    if (in.din)
        out << line ("lateness, net of DIN wire time", netStats) << "\n";

    char buffer[200];
    std::snprintf (buffer, sizeof (buffer), "drift (%s): %+.4f ms per minute; median first minute %.3f ms, last minute %.3f ms",
                   in.din ? "net" : "raw", slope, statsOf (firstMinute).median, statsOf (lastMinute).median);
    out << buffer << "\n";

    out << "messages sent " << messages.size() << ", received " << raw.size()
        << ", dropped " << dropped << ", unexpected " << unexpected
        << ", send() refused " << in.sender->failedSends()
        << ", input overflow " << in.inputOverflows
        << ", handed to backend after intended time " << handedLate << "\n";

    out << "\ntargets [D1] on " << (in.din ? "net" : "raw")
        << " lateness: p99 <= " << p99Target << " ms, max <= " << maxTarget << " ms, zero drops -> "
        << (met ? "MET" : "NOT MET") << "\n";

    if (in.din)
        out << "(wire time = the message's own bytes plus the bytes queued ahead of it on the same DIN line, 0.32 ms per byte)\n";

    report = out.str();
}

std::string TimingReport::csv() const
{
    const auto& messages = in.pattern->messages();
    const auto& sendCalls = in.sender->sendCallTimes();

    std::ostringstream out;
    out << "index,port,status,data1,data2,intended_ns,send_call_ns,received_ns,lateness_ns,wire_ns\n";

    for (std::size_t i = 0; i < messages.size(); ++i)
    {
        const auto& m = messages[i];
        const auto intended = in.startNanoseconds + m.offsetNanoseconds;
        out << i << ',' << m.port << ',' << int (m.bytes[0]) << ',' << int (m.bytes[1]) << ',' << int (m.bytes[2]) << ','
            << intended << ',' << sendCalls[i] << ',';

        if (outcomes[i].received)
            out << outcomes[i].receivedNanoseconds << ',' << (outcomes[i].receivedNanoseconds - intended);
        else
            out << ',';

        out << ',' << outcomes[i].wireNanoseconds << '\n';
    }

    return out.str();
}

} // namespace miditiming
