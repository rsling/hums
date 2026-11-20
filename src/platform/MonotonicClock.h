#pragma once

#include <cstdint>

namespace platform
{

// The engine's only notion of "now". Nanoseconds since an arbitrary, fixed
// epoch; never goes backwards. It is the time base MidiScheduler::send takes,
// so a time read here can be handed to the scheduler unchanged.
class MonotonicClock
{
public:
    virtual ~MonotonicClock() = default;

    // Safe on any thread, including the engine thread: no allocation, no lock.
    virtual std::int64_t nowNanoseconds() const noexcept = 0;
};

} // namespace platform
