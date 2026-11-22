// guardrails::SeedRange: which seeds a run of the property tests goes through
// (docs/GUARDRAILS.md -> 3).

#include <catch2/catch_test_macros.hpp>

#include "SeedRange.h"

TEST_CASE ("Without the variables the seeds are 1 to the configured number", "[guardrails][SeedRange]")
{
    const auto range = guardrails::SeedRange::fromTexts (nullptr, nullptr, 250);

    REQUIRE (range.has_value());
    CHECK (range->firstSeed == 1);
    CHECK (range->count == 250);
}

TEST_CASE ("The variables give the number of seeds and the first of them", "[guardrails][SeedRange]")
{
    const auto range = guardrails::SeedRange::fromTexts ("10000", "4711", 250);

    REQUIRE (range.has_value());
    CHECK (range->firstSeed == 4711);
    CHECK (range->count == 10000);

    const auto oneSeed = guardrails::SeedRange::fromTexts ("1", "0", 250);

    REQUIRE (oneSeed.has_value());
    CHECK (oneSeed->firstSeed == 0);
    CHECK (oneSeed->count == 1);

    const auto lastSeed = guardrails::SeedRange::fromTexts ("1", "18446744073709551615", 250);

    REQUIRE (lastSeed.has_value());
    CHECK (lastSeed->firstSeed == 18446744073709551615ULL);
}

TEST_CASE ("A variable that holds no such number gives no range", "[guardrails][SeedRange]")
{
    // No seeds at all would be a test that passes without having looked.
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("0", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts (nullptr, nullptr, 0).has_value());

    CHECK_FALSE (guardrails::SeedRange::fromTexts ("", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("many", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("-5", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("10 ", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("1e3", nullptr, 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts (nullptr, "0x10", 250).has_value());
    CHECK_FALSE (guardrails::SeedRange::fromTexts (nullptr, "", 250).has_value());

    // One more than the largest number of 64 bits.
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("18446744073709551616", nullptr, 250).has_value());

    // A range that would run past the largest seed.
    CHECK_FALSE (guardrails::SeedRange::fromTexts ("2", "18446744073709551615", 250).has_value());
}

TEST_CASE ("How to repeat a seed names both variables", "[guardrails][SeedRange]")
{
    CHECK (guardrails::SeedRange::howToRepeat (4711) == "HUMS_PROPERTY_FIRST_SEED=4711 HUMS_PROPERTY_SEEDS=1");
}
