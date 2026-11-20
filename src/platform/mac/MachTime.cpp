#include "platform/mac/MachTime.h"

#include <mach/mach_time.h>

namespace platform
{

MachTime::MachTime()
{
    mach_timebase_info_data_t info {};
    mach_timebase_info (&info);
    numerator = info.numer;
    denominator = info.denom;
}

std::uint64_t MachTime::nowHost() const noexcept
{
    return mach_absolute_time();
}

// Split into quotient and remainder so the multiplication can't overflow for
// any realistic uptime.
std::int64_t MachTime::hostToNanoseconds (std::uint64_t host) const noexcept
{
    const auto whole = host / denominator;
    const auto rest = host % denominator;
    return static_cast<std::int64_t> (whole * numerator + rest * numerator / denominator);
}

std::uint64_t MachTime::nanosecondsToHost (std::int64_t nanoseconds) const noexcept
{
    if (nanoseconds <= 0)
        return 0;

    const auto ns = static_cast<std::uint64_t> (nanoseconds);
    const auto whole = ns / numerator;
    const auto rest = ns % numerator;
    return whole * denominator + rest * denominator / numerator;
}

} // namespace platform
