#pragma once

#include <cstdint>

namespace platform
{

// Converts between mach host time (mach_absolute_time, the unit of CoreMIDI
// timestamps) and nanoseconds. Header is std-only; the source reads the
// timebase once at construction.
class MachTime
{
public:
    MachTime();

    std::uint64_t nowHost() const noexcept;
    std::int64_t hostToNanoseconds (std::uint64_t host) const noexcept;
    std::uint64_t nanosecondsToHost (std::int64_t nanoseconds) const noexcept;

private:
    // host × numerator / denominator = nanoseconds (125 / 3 on Apple silicon).
    std::uint32_t numerator = 1;
    std::uint32_t denominator = 1;
};

} // namespace platform
