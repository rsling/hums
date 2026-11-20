#include "platform/mac/MachMonotonicClock.h"

namespace platform
{

std::int64_t MachMonotonicClock::nowNanoseconds() const noexcept
{
    return machTime.hostToNanoseconds (machTime.nowHost());
}

} // namespace platform
