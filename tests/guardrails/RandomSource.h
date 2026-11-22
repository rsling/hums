#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace guardrails
{

// The random numbers of the property tests (docs/GUARDRAILS.md -> 3). Seeded:
// the same seed gives the same numbers on every run, so a failure can be
// repeated from the seed the test printed.
//
// std::mt19937_64 gives the same sequence for a seed in every standard
// library. The distributions of <random> do not promise that, so the numbers
// are brought into their range here, by a plain remainder. It favours the low
// end of a range by an amount nobody could measure, and nothing here needs an
// even spread.
class RandomSource
{
public:
    explicit RandomSource (std::uint64_t seed);

    // A whole number from lowest to highest, both included.
    std::int64_t between (std::int64_t lowest, std::int64_t highest);

    // The same for the many values of the schema that are an int.
    int intBetween (int lowest, int highest);

    // True in about `percent` of 100 calls.
    bool chance (int percent);

    // A position in a list of `size` entries, which must not be empty.
    std::size_t indexBelow (std::size_t size);

    template <typename Value>
    const Value& oneOf (const std::vector<Value>& values)
    {
        return values[indexBelow (values.size())];
    }

    template <typename Value>
    void shuffle (std::vector<Value>& values)
    {
        // From the back: each place gets one of the values not yet placed.
        for (auto last = values.size(); last > 1; --last)
            std::swap (values[last - 1], values[indexBelow (last)]);
    }

private:
    std::mt19937_64 engine;
};

} // namespace guardrails
