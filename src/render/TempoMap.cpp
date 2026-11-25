#include "render/TempoMap.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace render
{

namespace
{
    constexpr std::int64_t largestInt64 = std::numeric_limits<std::int64_t>::max();

    // 25 = 1000 nanoseconds per microsecond * 24 / 960 ticks per quarter.
    constexpr std::int64_t twentyFourthsPerTickAndMicrosecond = 25;

    // A bpm is read to five decimals.
    constexpr double hundredThousandthsPerBpm = 100000.0;

    // 60 000 000 microseconds per minute, for a bpm in hundred-thousandths.
    constexpr std::int64_t microsecondsPerMinuteTimesHundredThousand = 6'000'000'000'000;
} // namespace

std::int64_t TempoMap::microsecondsPerQuarterAt (double bpm) noexcept
{
    assert (std::isfinite (bpm));

    const std::int64_t hundredThousandths = std::llround (bpm * hundredThousandthsPerBpm);
    assert (hundredThousandths > 0);

    // Rounded to the nearest whole number, a half up: (2a + b) / 2b is
    // a / b + 1/2, and the division of whole numbers drops what is left.
    return (2 * microsecondsPerMinuteTimesHundredThousand + hundredThousandths) / (2 * hundredThousandths);
}

TempoMap::TempoMap (std::vector<Segment> segmentsInTickOrder)
{
    assert (! segmentsInTickOrder.empty());
    assert (segmentsInTickOrder.front().startTick == 0);

    timedSegments.reserve (segmentsInTickOrder.size());

    for (const Segment& segment : segmentsInTickOrder)
    {
        assert (segment.microsecondsPerQuarter >= fewestMicrosecondsPerQuarter);
        assert (segment.microsecondsPerQuarter <= mostMicrosecondsPerQuarter);

        TimedSegment timed;
        timed.startTick = segment.startTick;
        timed.twentyFourthsPerTick = TwentyFourths { segment.microsecondsPerQuarter } * twentyFourthsPerTickAndMicrosecond;

        if (! timedSegments.empty())
        {
            const TimedSegment& previous = timedSegments.back();
            assert (segment.startTick > previous.startTick);

            const TwentyFourths ticksOfPrevious = TwentyFourths { segment.startTick } - previous.startTick;
            timed.start = previous.start + ticksOfPrevious * previous.twentyFourthsPerTick;
        }

        timedSegments.push_back (timed);
    }
}

std::int64_t TempoMap::nanosecondsAt (std::int64_t tick) const noexcept
{
    const std::int64_t tickFromZero = std::max<std::int64_t> (tick, 0);
    const TimedSegment& segment = segmentAtTick (tickFromZero);

    const TwentyFourths ticksIntoSegment = TwentyFourths { tickFromZero } - segment.startTick;
    const TwentyFourths time = segment.start + ticksIntoSegment * segment.twentyFourthsPerTick;

    // To the nearest nanosecond, a half up.
    const TwentyFourths nanoseconds = (time + twentyFourthsPerNanosecond / 2) / twentyFourthsPerNanosecond;

    if (nanoseconds > largestInt64)
        return largestInt64;

    return static_cast<std::int64_t> (nanoseconds);
}

std::int64_t TempoMap::tickAt (std::int64_t nanoseconds) const noexcept
{
    const TwentyFourths time = TwentyFourths { std::max<std::int64_t> (nanoseconds, 0) } * twentyFourthsPerNanosecond;
    const TimedSegment& segment = segmentAtTime (time);

    // To the nearest tick, a half up: (2a + b) / 2b, as above. A time in the
    // second half of a segment's last tick gives the tick the next segment
    // starts at, which is the nearest.
    const TwentyFourths timeIntoSegment = time - segment.start;
    const TwentyFourths ticksIntoSegment = (2 * timeIntoSegment + segment.twentyFourthsPerTick)
                                         / (2 * segment.twentyFourthsPerTick);

    // Fits: a tick lasts 25 24ths of a nanosecond or more, so no time of 63
    // bits of nanoseconds is as late as a tick of 63 bits.
    return static_cast<std::int64_t> (segment.startTick + ticksIntoSegment);
}

const TempoMap::TimedSegment& TempoMap::segmentAtTick (std::int64_t tick) const noexcept
{
    const auto firstLater = std::upper_bound (timedSegments.begin(), timedSegments.end(), tick,
                                              [] (std::int64_t wanted, const TimedSegment& segment) { return wanted < segment.startTick; });

    // The first segment starts at tick 0, so there is always one before.
    return *std::prev (firstLater);
}

const TempoMap::TimedSegment& TempoMap::segmentAtTime (TwentyFourths time) const noexcept
{
    const auto firstLater = std::upper_bound (timedSegments.begin(), timedSegments.end(), time,
                                              [] (TwentyFourths wanted, const TimedSegment& segment) { return wanted < segment.start; });

    return *std::prev (firstLater);
}

} // namespace render
