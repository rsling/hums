#include "platform/mac/JuceTimerMidiScheduler.h"

namespace platform
{

JuceTimerMidiScheduler::JuceTimerMidiScheduler (const MonotonicClock& c)
    : clock (c)
{
}

JuceTimerMidiScheduler::~JuceTimerMidiScheduler()
{
    stopTimer();
}

MidiScheduler::PortHandle JuceTimerMidiScheduler::open (const std::string& deviceIdentifier)
{
    for (int handle = 0; handle < maxPorts; ++handle)
    {
        auto& output = outputs[static_cast<std::size_t> (handle)];

        if (output == nullptr)
        {
            // The timer never touches a slot that is still empty, so filling
            // one while it runs is safe.
            output = juce::MidiOutput::openDevice (juce::String (deviceIdentifier));

            if (output == nullptr)
                return invalidPort;

            if (! isTimerRunning())
                startTimer (1);

            return handle;
        }
    }

    return invalidPort;
}

void JuceTimerMidiScheduler::close (PortHandle port)
{
    if (port < 0 || port >= maxPorts)
        return;

    // stopTimer waits for a running callback, so the output can't be in use.
    stopTimer();

    int kept = 0;
    for (int i = 0; i < pendingCount; ++i)
        if (pending[static_cast<std::size_t> (i)].port != port)
            pending[static_cast<std::size_t> (kept++)] = pending[static_cast<std::size_t> (i)];
    pendingCount = kept;

    outputs[static_cast<std::size_t> (port)].reset();

    for (const auto& output : outputs)
        if (output != nullptr)
            return startTimer (1);
}

bool JuceTimerMidiScheduler::send (PortHandle port,
                                   std::span<const std::uint8_t> message,
                                   std::int64_t timeNanoseconds) noexcept
{
    if (port < 0 || port >= maxPorts || message.empty() || message.size() > 3 || message[0] == 0xf0)
        return false;

    Pending item;
    item.timeNanoseconds = timeNanoseconds;
    item.port = port;
    item.size = static_cast<std::uint8_t> (message.size());
    std::copy (message.begin(), message.end(), item.bytes.begin());

    const auto scope = queue.write (1);
    if (scope.blockSize1 + scope.blockSize2 != 1)
        return false;

    queueStorage[static_cast<std::size_t> (scope.blockSize1 == 1 ? scope.startIndex1 : scope.startIndex2)] = item;
    return true;
}

void JuceTimerMidiScheduler::takeQueuedMessages() noexcept
{
    const auto scope = queue.read (queue.getNumReady());

    const auto take = [this] (int start, int size)
    {
        for (int i = start; i < start + size; ++i)
        {
            const auto& item = queueStorage[static_cast<std::size_t> (i)];

            if (pendingCount == queueCapacity)
            {
                overflows.fetch_add (1);
                continue;
            }

            // Insert after every pending message at the same or an earlier
            // time; messages arrive almost sorted, so this is usually O(1).
            auto position = pendingCount;
            while (position > 0 && pending[static_cast<std::size_t> (position - 1)].timeNanoseconds > item.timeNanoseconds)
            {
                pending[static_cast<std::size_t> (position)] = pending[static_cast<std::size_t> (position - 1)];
                --position;
            }

            pending[static_cast<std::size_t> (position)] = item;
            ++pendingCount;
        }
    };

    take (scope.startIndex1, scope.blockSize1);
    take (scope.startIndex2, scope.blockSize2);
}

void JuceTimerMidiScheduler::hiResTimerCallback()
{
    takeQueuedMessages();

    const auto now = clock.nowNanoseconds();
    int sent = 0;

    while (sent < pendingCount && pending[static_cast<std::size_t> (sent)].timeNanoseconds <= now)
    {
        const auto& item = pending[static_cast<std::size_t> (sent)];

        if (auto& output = outputs[static_cast<std::size_t> (item.port)])
            output->sendMessageNow (juce::MidiMessage (item.bytes.data(), item.size));

        ++sent;
    }

    if (sent > 0)
    {
        std::copy (pending.begin() + sent, pending.begin() + pendingCount, pending.begin());
        pendingCount -= sent;
    }
}

} // namespace platform
