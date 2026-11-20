#pragma once

#include "platform/RealtimeThread.h"
#include "platform/mac/MachTime.h"

namespace platform
{

// THREAD_TIME_CONSTRAINT_POLICY on the calling thread: the mach scheduling
// class that CoreAudio's I/O threads use.
class MachRealtimeThread final : public RealtimeThread
{
public:
    bool raiseCallingThread (const Budget& budget) noexcept override;

private:
    MachTime machTime;
};

} // namespace platform
