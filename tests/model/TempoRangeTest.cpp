// model::TempoRange: the tempos Hums takes. docs/SCHEMA.md -> Timeline, [D23].

#include <catch2/catch_test_macros.hpp>

#include "model/CanonicalDouble.h"
#include "model/TempoRange.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{

// A tempo as TempoMap plays it and an SMF holds it [D13].
std::int64_t microsecondsPerQuarter (double bpm)
{
    return std::llround (60000000.0 / bpm);
}

} // namespace

TEST_CASE ("A tempo is 3.57628 to 999.999 bpm, both ends included", "[model][TempoRange]")
{
    CHECK (model::TempoRange::contains (3.57628));
    CHECK (model::TempoRange::contains (4.0));
    CHECK (model::TempoRange::contains (118.0));
    CHECK (model::TempoRange::contains (999.999));

    CHECK_FALSE (model::TempoRange::contains (3.57627));
    CHECK_FALSE (model::TempoRange::contains (1.0));
    CHECK_FALSE (model::TempoRange::contains (0.0));
    CHECK_FALSE (model::TempoRange::contains (-118.0));
    CHECK_FALSE (model::TempoRange::contains (1000.0));
    CHECK_FALSE (model::TempoRange::contains (std::numeric_limits<double>::max()));
}

TEST_CASE ("The ends are in the range also as the document holds them, read from their saved text", "[model][TempoRange]")
{
    CHECK (model::TempoRange::contains (juce::String ("3.57628").getDoubleValue()));
    CHECK (model::TempoRange::contains (juce::String ("999.999").getDoubleValue()));
    CHECK (model::TempoRange::contains (model::CanonicalDouble::rounded (3.576284)));
    CHECK (model::TempoRange::contains (model::CanonicalDouble::rounded (999.9994)));

    CHECK_FALSE (model::TempoRange::contains (juce::String ("3.57627").getDoubleValue()));
    CHECK_FALSE (model::TempoRange::contains (juce::String ("1000").getDoubleValue()));
    CHECK_FALSE (model::TempoRange::contains (model::CanonicalDouble::rounded (999.9996)));
}

TEST_CASE ("What is no number is no tempo", "[model][TempoRange]")
{
    CHECK_FALSE (model::TempoRange::contains (std::numeric_limits<double>::infinity()));
    CHECK_FALSE (model::TempoRange::contains (-std::numeric_limits<double>::infinity()));
    CHECK_FALSE (model::TempoRange::contains (std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE ("The lowest tempo is the slowest of six digits that a Standard MIDI File holds", "[model][TempoRange]")
{
    // 24 bits of microseconds per quarter note.
    CHECK (model::TempoRange::mostMicrosecondsPerQuarter == (std::int64_t { 1 } << 24) - 1);

    CHECK (microsecondsPerQuarter (model::TempoRange::lowestBpm) <= model::TempoRange::mostMicrosecondsPerQuarter);
    // The tempo one step of the sixth digit below it no longer fits.
    CHECK (microsecondsPerQuarter (3.57627) > model::TempoRange::mostMicrosecondsPerQuarter);

    // And the fastest tempo is far from the other end: no tempo in the range
    // plays as 0 microseconds.
    CHECK (microsecondsPerQuarter (model::TempoRange::highestBpm) == 60000);
}

TEST_CASE ("The range in words names both ends as they are saved", "[model][TempoRange]")
{
    CHECK (model::TempoRange::inWords() == "3.57628 to 999.999 bpm");
}
