#pragma once

#include <cstdint>
#include <vector>

namespace render
{

// The one authority for tick <-> time (docs/ARCHITECTURE.md -> Timing). It
// holds the Timeline's tempos as a list of segments and answers two
// questions: how long after tick 0 a tick is played, and which tick is played
// at a time after tick 0. The engine, the recorder and SMF export all ask
// here, so they can never drift apart.
//
// A tempo is played the way a Standard MIDI File holds it [D13]: as a whole
// number of microseconds per quarter note. Everything is worked out in whole
// numbers, from tick 0 each time, so no rounding adds up along the map.
//
// Std only, like the rest of the render core. The map allocates when it is
// made (message thread) and never afterwards: asking it is safe on the engine
// thread.
class TempoMap
{
public:
    static constexpr std::int64_t ticksPerQuarter = 960;

    // The range of an SMF tempo event: 24 bits, and not 0.
    static constexpr std::int64_t fewestMicrosecondsPerQuarter = 1;
    static constexpr std::int64_t mostMicrosecondsPerQuarter = 16777215;

    // One tempo, from its tick up to the next segment's tick; the last one
    // has no end.
    struct Segment
    {
        std::int64_t startTick = 0;
        std::int64_t microsecondsPerQuarter = 0;
    };

    // What a tempo of the document is played as: round (60 000 000 / bpm)
    // whole microseconds per quarter note [D13], a half rounded up (512 bpm
    // is 117187.5 and plays as 117188).
    //
    // The bpm is read to five decimals, which is all a tempo of the document
    // has (six significant digits, none below 3.57628 [D14], [D23]), and the
    // division is done in whole numbers: a division of doubles would decide a
    // half by the last bit of a number like 307.2, which no double holds
    // exactly. For a tempo of the document's range the result is 60 000 to
    // 16 777 210. A bpm that is no number, or that reads as 0 or less, is a
    // programming error and asserts.
    static std::int64_t microsecondsPerQuarterAt (double bpm) noexcept;

    // The segments as the Timeline has them: at least one, the first at tick
    // 0, the ticks strictly ascending, every tempo from
    // fewestMicrosecondsPerQuarter to mostMicrosecondsPerQuarter. The
    // Validator holds every document to that (invariants 6 and 10); anything
    // else here is a programming error and asserts.
    explicit TempoMap (std::vector<Segment> segmentsInTickOrder);

    // The time at which a tick is played, in nanoseconds after tick 0,
    // rounded to the nearest nanosecond, a half rounded up. A tick before 0
    // is taken as tick 0. A time too large for the return type is returned as
    // the largest value it holds, some 292 years after tick 0.
    std::int64_t nanosecondsAt (std::int64_t tick) const noexcept;

    // The tick that is played nearest to a time given in nanoseconds after
    // tick 0; a time exactly halfway between two ticks belongs to the later
    // one. A time before 0 is taken as 0.
    //
    // tickAt (nanosecondsAt (tick)) is that tick again, for every tick whose
    // time fits the return type of nanosecondsAt: a tick is never shorter
    // than 25/24 of a nanosecond, and the rounding of the time moves it by
    // half a nanosecond at most.
    std::int64_t tickAt (std::int64_t nanoseconds) const noexcept;

private:
    // Time in 24ths of a nanosecond. A tick lasts microsecondsPerQuarter *
    // 1000 / 960 nanoseconds, which is microsecondsPerQuarter * 25 of these,
    // a whole number, so that the time of every tick is exact.
    //
    // 128 bits wide, because the largest tick of a document (63 bits) at the
    // slowest tempo (24 bits) does not fit 64. The type is the compiler's
    // own, not the standard's; it needs no header.
    using TwentyFourths = __int128;

    static constexpr TwentyFourths twentyFourthsPerNanosecond = 24;

    // A segment together with the exact time at which it begins.
    struct TimedSegment
    {
        std::int64_t startTick = 0;
        TwentyFourths start = 0;
        TwentyFourths twentyFourthsPerTick = 0;
    };

    // The last segment that begins at or before the tick; the time.
    const TimedSegment& segmentAtTick (std::int64_t tick) const noexcept;
    const TimedSegment& segmentAtTime (TwentyFourths time) const noexcept;

    std::vector<TimedSegment> timedSegments;
};

} // namespace render
