#pragma once

#include "platform/MonotonicClock.h"
#include "platform/mac/MachTime.h"

namespace platform
{

// mach_absolute_time in nanoseconds: the same clock CoreMIDI timestamps use,
// so CoreMidiScheduler can convert without drift. Stops while the Mac sleeps.
class MachMonotonicClock final : public MonotonicClock
{
public:
    std::int64_t nowNanoseconds() const noexcept override;

private:
    MachTime machTime;
};

} // namespace platform
