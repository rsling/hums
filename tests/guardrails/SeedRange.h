#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace guardrails
{

// Which seeds a run of the property tests goes through
// (docs/GUARDRAILS.md -> 3).
//
// Two environment variables choose, because CTest hands no arguments on to a
// test:
//
//   HUMS_PROPERTY_SEEDS       how many seeds; without it the number the build
//                             was configured with, which is smaller in the
//                             instrumented configurations
//   HUMS_PROPERTY_FIRST_SEED  the first of them; without it 1
//
// A seed a failure has printed is run again alone with
// HUMS_PROPERTY_FIRST_SEED=<seed> HUMS_PROPERTY_SEEDS=1.
struct SeedRange
{
    std::uint64_t firstSeed = 1;
    std::uint64_t count = 0;

    // The range the environment asks for. `countWhenNotSet` is the number of
    // seeds when HUMS_PROPERTY_SEEDS is not set. Nothing when a variable is
    // set to something that is no such number (a count is 1 or more, a seed 0
    // or more, and the range must not run past the largest seed): the test
    // then fails rather than run some other seeds than were asked for.
    static std::optional<SeedRange> fromEnvironment (std::uint64_t countWhenNotSet);

    // The same from the two texts, each nullptr when its variable is not set.
    static std::optional<SeedRange> fromTexts (const char* countText, const char* firstSeedText, std::uint64_t countWhenNotSet);

    // How to run one seed again, for the message of a failure.
    static std::string howToRepeat (std::uint64_t seed);
};

} // namespace guardrails
