// guardrails::RandomSource: the seeded random numbers of the property tests
// (docs/GUARDRAILS.md -> 3).

#include <catch2/catch_test_macros.hpp>

#include "RandomSource.h"

#include <cstdint>
#include <limits>
#include <set>
#include <vector>

TEST_CASE ("The same seed gives the same numbers", "[guardrails][RandomSource]")
{
    guardrails::RandomSource first (42);
    guardrails::RandomSource second (42);

    for (int draw = 0; draw < 1000; ++draw)
        REQUIRE (first.between (-1000000, 1000000) == second.between (-1000000, 1000000));
}

TEST_CASE ("The numbers are those std::mt19937_64 is specified to give", "[guardrails][RandomSource]")
{
    // The C++ standard fixes the 10000th number of a default-constructed
    // std::mt19937_64, whose seed is 5489. If this holds, a seed means the
    // same numbers with every standard library.
    guardrails::RandomSource random (5489);
    const auto smallest = std::numeric_limits<std::int64_t>::min();
    const auto largest = std::numeric_limits<std::int64_t>::max();

    for (int draw = 1; draw < 10000; ++draw)
        random.between (smallest, largest);

    // Over the whole range the engine's number is the distance from the
    // smallest value, in the arithmetic of 64 bits.
    const auto distance = static_cast<std::uint64_t> (random.between (smallest, largest)) - static_cast<std::uint64_t> (smallest);

    CHECK (distance == 9981545732273789042ULL);
}

TEST_CASE ("Another seed gives other numbers", "[guardrails][RandomSource]")
{
    guardrails::RandomSource first (1);
    guardrails::RandomSource second (2);

    int differing = 0;

    for (int draw = 0; draw < 100; ++draw)
        if (first.between (0, 1000000) != second.between (0, 1000000))
            ++differing;

    CHECK (differing > 90);
}

TEST_CASE ("A number lies in its range, and both ends of the range occur", "[guardrails][RandomSource]")
{
    guardrails::RandomSource random (7);
    std::set<std::int64_t> seen;

    for (int draw = 0; draw < 1000; ++draw)
    {
        const auto number = random.between (-2, 3);

        REQUIRE (number >= -2);
        REQUIRE (number <= 3);
        seen.insert (number);
    }

    CHECK (seen == std::set<std::int64_t> { -2, -1, 0, 1, 2, 3 });
    CHECK (random.between (5, 5) == 5);
    CHECK (random.intBetween (-9, -9) == -9);
}

TEST_CASE ("A chance of 0 never comes true and one of 100 always", "[guardrails][RandomSource]")
{
    guardrails::RandomSource random (7);

    for (int draw = 0; draw < 1000; ++draw)
    {
        REQUIRE_FALSE (random.chance (0));
        REQUIRE (random.chance (100));
    }
}

TEST_CASE ("A shuffled list holds what it held before", "[guardrails][RandomSource]")
{
    guardrails::RandomSource random (7);
    const std::vector<int> original { 1, 2, 3, 4, 5, 6, 7, 8 };

    auto shuffled = original;
    random.shuffle (shuffled);

    CHECK (shuffled != original);
    CHECK (std::multiset<int> (shuffled.begin(), shuffled.end()) == std::multiset<int> (original.begin(), original.end()));

    std::vector<int> empty;
    random.shuffle (empty);
    CHECK (empty.empty());
}

TEST_CASE ("One of a list is a value of that list", "[guardrails][RandomSource]")
{
    guardrails::RandomSource random (7);
    const std::vector<int> values { 10, 20, 30 };
    std::set<int> seen;

    for (int draw = 0; draw < 200; ++draw)
        seen.insert (random.oneOf (values));

    CHECK (seen == std::set<int> { 10, 20, 30 });
}
