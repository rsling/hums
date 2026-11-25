#include "model/Quantise.h"

#include <juce_core/juce_core.h>

#include <algorithm>

namespace model
{

std::int64_t Quantise::onsetOf (std::int64_t tick, std::int64_t grid, std::int64_t loopLength)
{
    jassert (tick >= 0 && grid >= 0 && loopLength > 0);

    if (grid <= 0 || tick < 0 || tick >= loopLength)
        return tick;

    // The line at or before the onset. It is a tick of the loop, since it is
    // not past the onset.
    const std::int64_t distanceDown = tick % grid;
    const std::int64_t lineBefore = tick - distanceDown;

    // The candidate after the onset is the next line or the loop point,
    // whichever comes first. Both are measured as distances from the onset:
    // the next line itself may lie past the largest tick there is.
    const std::int64_t distanceToNextLine = grid - distanceDown;
    const std::int64_t distanceToLoopPoint = loopLength - tick;
    const std::int64_t distanceUp = std::min (distanceToNextLine, distanceToLoopPoint);

    // A tie goes down.
    if (distanceDown <= distanceUp)
        return lineBefore;

    // The loop point stands for tick 0, also where a line falls on it.
    if (distanceToLoopPoint <= distanceToNextLine)
        return 0;

    return tick + distanceToNextLine;
}

} // namespace model
