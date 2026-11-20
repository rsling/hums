#include "InputRecorder.h"

namespace miditiming
{

InputRecorder::InputRecorder (const platform::MonotonicClock& c, std::size_t capacity)
    : clock (c), storage (capacity)
{
}

void InputRecorder::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    const auto now = clock.nowNanoseconds();
    const auto* data = message.getRawData();
    const auto size = message.getRawDataSize();

    if (size < 1 || size > 3 || data[0] >= 0xf0)
        return;

    // One CoreMIDI thread calls this per input: single producer.
    const auto index = count.load (std::memory_order_relaxed);
    if (index >= storage.size())
    {
        overflows.fetch_add (1);
        return;
    }

    auto& slot = storage[index];
    slot.timeNanoseconds = now;
    slot.size = static_cast<std::uint8_t> (size);
    for (int i = 0; i < size; ++i)
        slot.bytes[static_cast<std::size_t> (i)] = data[i];

    count.store (index + 1, std::memory_order_release);
}

std::vector<ReceivedMessage> InputRecorder::received() const
{
    const auto n = count.load (std::memory_order_acquire);
    return { storage.begin(), storage.begin() + static_cast<std::ptrdiff_t> (n) };
}

} // namespace miditiming
