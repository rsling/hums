#include "Sender.h"

#include <algorithm>
#include <chrono>
#include <span>
#include <thread>

namespace miditiming
{

Sender::Sender (platform::MidiScheduler& s,
                const platform::MonotonicClock& c,
                platform::RealtimeThread& r,
                const TrafficPattern& p,
                std::vector<platform::MidiScheduler::PortHandle> portHandles,
                Settings st)
    : scheduler (s), clock (c), realtime (r), pattern (p), ports (std::move (portHandles)), settings (st)
{
    const auto windows = (pattern.lengthNanoseconds() + settings.lookAheadNanoseconds) / settings.windowNanoseconds + 16;
    wakeLog.reserve (static_cast<std::size_t> (windows));
    sendLog.resize (pattern.messages().size(), 0);
}

void Sender::run (std::int64_t startNanoseconds)
{
    platform::RealtimeThread::Budget budget;
    budget.periodNanoseconds = settings.windowNanoseconds;
    budget.computationNanoseconds = std::max<std::int64_t> (settings.windowNanoseconds / 4, 200'000);
    budget.constraintNanoseconds = settings.windowNanoseconds;
    granted = realtime.raiseCallingThread (budget);

    const auto& messages = pattern.messages();
    std::size_t next = 0;

    // The first window opens one look-ahead before time zero, as a play
    // command would.
    auto intendedWake = startNanoseconds - settings.lookAheadNanoseconds;

    while (next < messages.size())
    {
        const auto before = clock.nowNanoseconds();
        if (before < intendedWake)
            std::this_thread::sleep_for (std::chrono::nanoseconds (intendedWake - before));

        const auto woke = clock.nowNanoseconds();
        if (wakeLog.size() < wakeLog.capacity())
            wakeLog.push_back ({ intendedWake, woke });

        const auto horizon = woke + settings.lookAheadNanoseconds;

        while (next < messages.size() && startNanoseconds + messages[next].offsetNanoseconds < horizon)
        {
            const auto& m = messages[next];
            sendLog[next] = clock.nowNanoseconds();

            if (! scheduler.send (ports[static_cast<std::size_t> (m.port)],
                                  std::span<const std::uint8_t> (m.bytes.data(), m.size),
                                  startNanoseconds + m.offsetNanoseconds))
                ++failed;

            ++next;
        }

        intendedWake += settings.windowNanoseconds;
    }
}

} // namespace miditiming
