// model::Quantise: where a clip's quantise setting puts a note onset.
// docs/SCHEMA.md -> ClipPool / Clip.

#include <catch2/catch_test_macros.hpp>

#include "model/Quantise.h"

#include <cstdint>
#include <limits>

using model::Quantise;

TEST_CASE ("An onset goes to the nearest grid line", "[model][Quantise]")
{
    // 1/8: a grid of 480 ticks, in a loop of one bar.
    CHECK (Quantise::onsetOf (10, 480, 3840) == 0);
    CHECK (Quantise::onsetOf (239, 480, 3840) == 0);
    CHECK (Quantise::onsetOf (241, 480, 3840) == 480);
    CHECK (Quantise::onsetOf (470, 480, 3840) == 480);
    CHECK (Quantise::onsetOf (500, 480, 3840) == 480);
    CHECK (Quantise::onsetOf (1000, 480, 3840) == 960);
    CHECK (Quantise::onsetOf (3500, 480, 3840) == 3360);
}

TEST_CASE ("An onset on a grid line stays there", "[model][Quantise]")
{
    CHECK (Quantise::onsetOf (0, 480, 3840) == 0);
    CHECK (Quantise::onsetOf (480, 480, 3840) == 480);
    CHECK (Quantise::onsetOf (3360, 480, 3840) == 3360);
}

TEST_CASE ("An onset exactly between two grid lines goes to the earlier one", "[model][Quantise]")
{
    CHECK (Quantise::onsetOf (240, 480, 3840) == 0);
    CHECK (Quantise::onsetOf (720, 480, 3840) == 480);
    // 1/8T: a grid of 320 ticks.
    CHECK (Quantise::onsetOf (160, 320, 1920) == 0);
    CHECK (Quantise::onsetOf (480, 320, 1920) == 320);
}

TEST_CASE ("Every grid of the schema is measured from clip tick 0", "[model][Quantise]")
{
    const std::int64_t loopLength = 7680;

    // 1/2, 1/4, 1/8, 1/2T, 1/4T, 1/8T.
    for (const std::int64_t grid : { 1920, 960, 480, 1280, 640, 320 })
    {
        INFO ("grid " << grid);
        CHECK (Quantise::onsetOf (grid - 1, grid, loopLength) == grid);
        CHECK (Quantise::onsetOf (grid + 1, grid, loopLength) == grid);
        CHECK (Quantise::onsetOf (grid + grid / 2, grid, loopLength) == grid);
        CHECK (Quantise::onsetOf (grid + grid / 2 + 1, grid, loopLength) == 2 * grid);
    }
}

TEST_CASE ("An onset nearer to the loop point than to the last grid line wraps to tick 0", "[model][Quantise]")
{
    // The loop is a multiple of the grid: its end is a line, and stands for 0.
    CHECK (Quantise::onsetOf (1700, 480, 1920) == 0);
    CHECK (Quantise::onsetOf (1919, 480, 1920) == 0);
    CHECK (Quantise::onsetOf (1679, 480, 1920) == 1440);
}

TEST_CASE ("In a loop that is no multiple of the grid, the loop point is a candidate and no line is invented past it", "[model][Quantise]")
{
    // Lines at 0, 480 and 960; the loop point 1000 stands for 0. The next
    // multiple, 1440, is no candidate.
    CHECK (Quantise::onsetOf (990, 480, 1000) == 0);
    CHECK (Quantise::onsetOf (981, 480, 1000) == 0);
    CHECK (Quantise::onsetOf (979, 480, 1000) == 960);
    CHECK (Quantise::onsetOf (960, 480, 1000) == 960);
    CHECK (Quantise::onsetOf (700, 480, 1000) == 480);

    // Between the last line and the loop point a tie goes down, to the line.
    CHECK (Quantise::onsetOf (980, 480, 1000) == 960);
}

TEST_CASE ("In a loop shorter than the grid every onset goes to tick 0", "[model][Quantise]")
{
    // The only candidates are 0 and the loop point, which stands for 0.
    CHECK (Quantise::onsetOf (0, 480, 300) == 0);
    CHECK (Quantise::onsetOf (150, 480, 300) == 0);
    CHECK (Quantise::onsetOf (151, 480, 300) == 0);
    CHECK (Quantise::onsetOf (299, 480, 300) == 0);
    CHECK (Quantise::onsetOf (0, 1920, 1) == 0);
}

TEST_CASE ("A quantised onset is a tick of the loop", "[model][Quantise]")
{
    for (const std::int64_t loopLength : { 1, 7, 319, 320, 321, 1000, 1920, 2000 })
    {
        for (const std::int64_t grid : { 1920, 960, 480, 1280, 640, 320 })
        {
            for (std::int64_t tick = 0; tick < loopLength; ++tick)
            {
                const std::int64_t onset = Quantise::onsetOf (tick, grid, loopLength);

                INFO ("tick " << tick << ", grid " << grid << ", loop length " << loopLength);
                REQUIRE (onset >= 0);
                REQUIRE (onset < loopLength);
                REQUIRE (onset % grid == 0);
            }
        }
    }
}

TEST_CASE ("A note at or past the loop length is not quantised", "[model][Quantise]")
{
    CHECK (Quantise::onsetOf (1920, 480, 1920) == 1920);
    CHECK (Quantise::onsetOf (2000, 480, 1920) == 2000);
    CHECK (Quantise::onsetOf (5010, 480, 1920) == 5010);
}

TEST_CASE ("With the grid off an onset is the stored tick", "[model][Quantise]")
{
    CHECK (Quantise::onsetOf (0, 0, 1920) == 0);
    CHECK (Quantise::onsetOf (250, 0, 1920) == 250);
    CHECK (Quantise::onsetOf (5010, 0, 1920) == 5010);
}

TEST_CASE ("Quantising holds at the largest ticks there are", "[model][Quantise]")
{
    const std::int64_t largest = std::numeric_limits<std::int64_t>::max();

    // The last line before the largest tick, which lies 127 ticks past it,
    // and the tick 10 past that line: the line after it would lie past the
    // largest tick, and is never formed.
    const std::int64_t lastLine = largest - largest % 1920;
    REQUIRE (largest - lastLine == 127);
    CHECK (Quantise::onsetOf (lastLine + 10, 1920, largest) == lastLine);

    // One tick before the loop point: nearer to it than to the last line.
    CHECK (Quantise::onsetOf (largest - 1, 1920, largest) == 0);
}
