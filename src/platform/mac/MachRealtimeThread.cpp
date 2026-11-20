#include "platform/mac/MachRealtimeThread.h"

#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>

namespace platform
{

bool MachRealtimeThread::raiseCallingThread (const Budget& budget) noexcept
{
    thread_time_constraint_policy_data_t policy {};
    policy.period = static_cast<uint32_t> (machTime.nanosecondsToHost (budget.periodNanoseconds));
    policy.computation = static_cast<uint32_t> (machTime.nanosecondsToHost (budget.computationNanoseconds));
    policy.constraint = static_cast<uint32_t> (machTime.nanosecondsToHost (budget.constraintNanoseconds));
    policy.preemptible = 1;

    const auto result = thread_policy_set (pthread_mach_thread_np (pthread_self()),
                                           THREAD_TIME_CONSTRAINT_POLICY,
                                           reinterpret_cast<thread_policy_t> (&policy),
                                           THREAD_TIME_CONSTRAINT_POLICY_COUNT);
    return result == KERN_SUCCESS;
}

} // namespace platform
