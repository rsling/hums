// TempoMap: tick <-> nanoseconds after tick 0, every tempo played as whole
// microseconds per quarter note [D13] (docs/ARCHITECTURE.md -> Timing).
//
// The expected numbers are worked out by hand. A tick at m microseconds per
// quarter note lasts m * 1000 / 960 = m * 25 / 24 nanoseconds: at 120 bpm
// (m = 500 000) that is 520 833 1/3 ns, and twelve ticks are 6 250 000 ns.

#include <catch2/catch_test_macros.hpp>

#include "render/TempoMap.h"

#include <cstdint>
#include <limits>
#include <vector>

using render::TempoMap;

namespace
{
constexpr std::int64_t largestInt64 = std::numeric_limits<std::int64_t>::max();

constexpr std::int64_t at120Bpm = 500000;
constexpr std::int64_t at118Bpm = 508475;
constexpr std::int64_t at60Bpm = 1000000;

TempoMap oneTempo (std::int64_t microsecondsPerQuarter)
{
    return TempoMap ({ { 0, microsecondsPerQuarter } });
}
} // namespace

TEST_CASE ("A tempo is played as round (60 000 000 / bpm) microseconds per quarter note", "[render][tempomap]")
{
    CHECK (TempoMap::microsecondsPerQuarterAt (120.0) == 500000);
    CHECK (TempoMap::microsecondsPerQuarterAt (60.0) == 1000000);
    CHECK (TempoMap::microsecondsPerQuarterAt (100.0) == 600000);

    // 508 474.576... rounds up, 662 983.425... rounds down.
    CHECK (TempoMap::microsecondsPerQuarterAt (118.0) == 508475);
    CHECK (TempoMap::microsecondsPerQuarterAt (90.5) == 662983);
}

TEST_CASE ("A tempo of exactly half a microsecond more is rounded up", "[render][tempomap]")
{
    // 60 000 000 / 512 = 117 187.5
    CHECK (TempoMap::microsecondsPerQuarterAt (512.0) == 117188);

    // 60 000 000 / 307.2 = 195 312.5, and no double is exactly 307.2: the
    // bpm is read as 307.20000, not as the double nearest to it.
    CHECK (TempoMap::microsecondsPerQuarterAt (307.2) == 195313);
}

TEST_CASE ("Both ends of the document's tempo range fit an SMF tempo event", "[render][tempomap]")
{
    // [D23]: 60 000 000 / 3.57628 = 16 777 209.8..., the slowest tempo of six
    // digits that fits 24 bits; 60 000 000 / 999.999 = 60 000.06...
    CHECK (TempoMap::microsecondsPerQuarterAt (3.57628) == 16777210);
    CHECK (TempoMap::microsecondsPerQuarterAt (999.999) == 60000);

    CHECK (TempoMap::microsecondsPerQuarterAt (3.57628) <= TempoMap::mostMicrosecondsPerQuarter);
    CHECK (TempoMap::microsecondsPerQuarterAt (999.999) >= TempoMap::fewestMicrosecondsPerQuarter);
}

TEST_CASE ("A tick's time under one tempo", "[render][tempomap]")
{
    const TempoMap map = oneTempo (at120Bpm);

    CHECK (map.nanosecondsAt (0) == 0);
    CHECK (map.nanosecondsAt (12) == 6250000);
    CHECK (map.nanosecondsAt (960) == 500000000);
    CHECK (map.nanosecondsAt (1920) == 1000000000);

    // A quarter note at 118 bpm is 508 475 microseconds, not 508 474.576.
    CHECK (oneTempo (at118Bpm).nanosecondsAt (960) == 508475000);
}

TEST_CASE ("A tick's time is rounded to the nearest nanosecond, a half up", "[render][tempomap]")
{
    const TempoMap map = oneTempo (at120Bpm);

    CHECK (map.nanosecondsAt (1) == 520833); // 520 833 1/3
    CHECK (map.nanosecondsAt (2) == 1041667); // 1 041 666 2/3

    // 508 475 * 25 / 24 = 529 661.458...; seven ticks 3 707 630.208...
    CHECK (oneTempo (at118Bpm).nanosecondsAt (1) == 529661);
    CHECK (oneTempo (at118Bpm).nanosecondsAt (7) == 3707630);

    // 12 microseconds per quarter: a tick is 12.5 ns.
    CHECK (oneTempo (12).nanosecondsAt (1) == 13);
    CHECK (oneTempo (12).nanosecondsAt (3) == 38);
}

TEST_CASE ("A tick's time across tempo changes", "[render][tempomap]")
{
    // 120 bpm for two quarters (1 s), 118 bpm for one (0.508475 s), then the
    // slowest tempo of the document, 16 777 210 microseconds per quarter.
    const TempoMap map ({ { 0, at120Bpm }, { 1920, at118Bpm }, { 2880, 16777210 } });

    CHECK (map.nanosecondsAt (1919) == 999479167); // 1 s less 520 833 1/3
    CHECK (map.nanosecondsAt (1920) == 1000000000);
    CHECK (map.nanosecondsAt (1921) == 1000529661);
    CHECK (map.nanosecondsAt (2880) == 1508475000);

    // One tick there is 16 777 210 * 25 / 24 = 17 476 260.41... ns.
    CHECK (map.nanosecondsAt (2881) == 1525951260);
    CHECK (map.nanosecondsAt (2880 + 960) == 1508475000 + 16777210000);
}

TEST_CASE ("Rounding does not add up along the tempo map", "[render][tempomap]")
{
    // Twenty-four segments of one tick each, all at 120 bpm. Each tick is
    // 520 833 1/3 ns; rounded one by one they would come to 12 499 992.
    std::vector<TempoMap::Segment> segments;
    for (std::int64_t tick = 0; tick < 24; ++tick)
        segments.push_back ({ tick, at120Bpm });

    const TempoMap map (segments);

    CHECK (map.nanosecondsAt (24) == 12500000);
    CHECK (map.nanosecondsAt (960) == 500000000);
}

TEST_CASE ("The tick at a time under one tempo", "[render][tempomap]")
{
    const TempoMap map = oneTempo (at120Bpm);

    CHECK (map.tickAt (0) == 0);
    CHECK (map.tickAt (6250000) == 12);
    CHECK (map.tickAt (500000000) == 960);
    CHECK (map.tickAt (1000000000) == 1920);
}

TEST_CASE ("The tick at a time is the nearest tick", "[render][tempomap]")
{
    const TempoMap map = oneTempo (at120Bpm);

    // Halfway between tick 0 and tick 1 is 260 416 2/3 ns.
    CHECK (map.tickAt (260416) == 0);
    CHECK (map.tickAt (260417) == 1);

    // Tick 1 is at 520 833 1/3, halfway to tick 2 is 781 250.
    CHECK (map.tickAt (520833) == 1);
    CHECK (map.tickAt (520834) == 1);
    CHECK (map.tickAt (781249) == 1);
}

TEST_CASE ("A time exactly halfway between two ticks belongs to the later one", "[render][tempomap]")
{
    // At 120 bpm three ticks are 1 562 500 ns, so tick 1.5 is at 781 250.
    CHECK (oneTempo (at120Bpm).tickAt (781250) == 2);

    // 48 microseconds per quarter: a tick is 50 ns.
    const TempoMap map = oneTempo (48);

    CHECK (map.tickAt (24) == 0);
    CHECK (map.tickAt (25) == 1);
    CHECK (map.tickAt (74) == 1);
    CHECK (map.tickAt (75) == 2);
}

TEST_CASE ("The tick at a time across tempo changes", "[render][tempomap]")
{
    // 120 bpm for two quarters (1 s), then 60 bpm: a tick is 1 041 666 2/3 ns.
    const TempoMap map ({ { 0, at120Bpm }, { 1920, at60Bpm } });

    CHECK (map.tickAt (1000000000) == 1920);
    CHECK (map.tickAt (2000000000) == 2880);

    // The last tick before the change is 520 833 1/3 ns long; a time in its
    // second half is nearest to the tick of the change.
    CHECK (map.tickAt (999739583) == 1919); // halfway is 999 739 583 1/3
    CHECK (map.tickAt (999739584) == 1920);

    // After the change half a tick is 520 833 1/3 ns.
    CHECK (map.tickAt (1000520833) == 1920);
    CHECK (map.tickAt (1000520834) == 1921);
}

TEST_CASE ("The tick at a tick's time is that tick again", "[render][tempomap]")
{
    const TempoMap map ({ { 0, at120Bpm }, { 1920, at118Bpm }, { 2880, 16777210 }, { 2881, 60000 }, { 5000, at60Bpm } });

    for (std::int64_t tick = 0; tick < 12000; ++tick)
    {
        INFO ("tick " << tick);
        REQUIRE (map.tickAt (map.nanosecondsAt (tick)) == tick);
    }

    // Also far along the timeline (some 16 years at 60 bpm), and for the
    // shortest tick there is, 25/24 ns.
    CHECK (map.tickAt (map.nanosecondsAt (500000000000)) == 500000000000);

    const TempoMap fastest = oneTempo (TempoMap::fewestMicrosecondsPerQuarter);
    for (std::int64_t tick = 0; tick < 100; ++tick)
    {
        INFO ("tick " << tick);
        REQUIRE (fastest.tickAt (fastest.nanosecondsAt (tick)) == tick);
    }
}

TEST_CASE ("A later tick is never at an earlier time", "[render][tempomap]")
{
    const TempoMap map ({ { 0, at120Bpm }, { 7, TempoMap::fewestMicrosecondsPerQuarter }, { 40, at118Bpm } });

    for (std::int64_t tick = 0; tick < 100; ++tick)
    {
        INFO ("tick " << tick);
        REQUIRE (map.nanosecondsAt (tick + 1) >= map.nanosecondsAt (tick));
    }
}

TEST_CASE ("A tick or a time before 0 is taken as 0", "[render][tempomap]")
{
    const TempoMap map = oneTempo (at120Bpm);

    CHECK (map.nanosecondsAt (-1) == 0);
    CHECK (map.nanosecondsAt (std::numeric_limits<std::int64_t>::min()) == 0);
    CHECK (map.tickAt (-1) == 0);
    CHECK (map.tickAt (std::numeric_limits<std::int64_t>::min()) == 0);
}

TEST_CASE ("A time too large for 64 bits is the largest there is", "[render][tempomap]")
{
    // The largest tick of a document, at 120 bpm and at the slowest tempo an
    // SMF holds, also behind a tempo change.
    CHECK (oneTempo (at120Bpm).nanosecondsAt (largestInt64) == largestInt64);
    CHECK (oneTempo (TempoMap::mostMicrosecondsPerQuarter).nanosecondsAt (largestInt64) == largestInt64);

    const TempoMap changed ({ { 0, at120Bpm }, { largestInt64 - 1, TempoMap::mostMicrosecondsPerQuarter } });
    CHECK (changed.nanosecondsAt (largestInt64 - 1) == largestInt64);
    CHECK (changed.nanosecondsAt (largestInt64) == largestInt64);

    // Well before that the time is exact: 10^12 ticks at 120 bpm are
    // 520 833 333 333 333 333 1/3 ns.
    CHECK (oneTempo (at120Bpm).nanosecondsAt (1000000000000) == 520833333333333333);
}

TEST_CASE ("The tick at the largest time there is", "[render][tempomap]")
{
    // At one microsecond per quarter a tick is 25/24 ns, the shortest there
    // is: the largest time times 24 / 25 is 8 854 437 155 380 584 774.72.
    const TempoMap fastest = oneTempo (TempoMap::fewestMicrosecondsPerQuarter);
    CHECK (fastest.tickAt (largestInt64) == 8854437155380584775);

    // At 120 bpm: 9 223 372 036 854 775 807 * 24 / 12 500 000 =
    // 17 708 874 310 761.16...
    CHECK (oneTempo (at120Bpm).tickAt (largestInt64) == 17708874310761);
}
