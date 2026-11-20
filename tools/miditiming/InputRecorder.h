#pragma once

#include "platform/MonotonicClock.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace miditiming
{

struct ReceivedMessage
{
    std::int64_t timeNanoseconds = 0;
    std::array<std::uint8_t, 3> bytes {};
    std::uint8_t size = 0;
    // Which input it arrived on, for listen mode with several inputs: arrival
    // order means something only between messages of one input.
    int input = 0;
};

// Records what arrives on one input. The arrival time is read from the
// MonotonicClock inside JUCE's callback, not taken from the message: JUCE
// 9.0.6 maps CoreMIDI's packet time onto Time::getMillisecondCounter(), whose
// anchor is a whole millisecond (juce_CoreMidi_mac.mm, TimeConverter), which
// would add an unknown constant of up to 1 ms. The callback time includes
// CoreMIDI's delivery to the callback, so lateness is slightly overstated:
// it errs on the safe side. Real-time messages (clock, active sensing) and
// SysEx are ignored, since the pattern sends neither.
class InputRecorder final : public juce::MidiInputCallback
{
public:
    InputRecorder (const platform::MonotonicClock& clock, std::size_t capacity);

    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;

    // Call only after the input has been stopped.
    std::vector<ReceivedMessage> received() const;
    int overflowCount() const { return overflows.load(); }
    // How many messages have arrived so far; safe while the input runs.
    std::size_t receivedCount() const { return count.load (std::memory_order_acquire); }

private:
    const platform::MonotonicClock& clock;
    std::vector<ReceivedMessage> storage;
    std::atomic<std::size_t> count { 0 };
    std::atomic<int> overflows { 0 };
};

} // namespace miditiming
