#pragma once

#include <cstdint>

namespace platform
{

// Raises the calling thread to real-time priority, for the engine thread.
class RealtimeThread
{
public:
    // What the thread promises the scheduler, in nanoseconds: it wakes about
    // once per period, needs at most computation of CPU time per wake, and
    // must have finished within constraint of waking.
    struct Budget
    {
        std::int64_t periodNanoseconds = 0;
        std::int64_t computationNanoseconds = 0;
        std::int64_t constraintNanoseconds = 0;
    };

    virtual ~RealtimeThread() = default;

    // Returns false if the OS refused; the thread then keeps its old priority.
    virtual bool raiseCallingThread (const Budget& budget) noexcept = 0;
};

} // namespace platform
