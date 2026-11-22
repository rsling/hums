#include "RandomSource.h"

#include <cassert>

namespace guardrails
{

RandomSource::RandomSource (std::uint64_t seed)
    : engine (seed)
{
}

std::int64_t RandomSource::between (std::int64_t lowest, std::int64_t highest)
{
    assert (lowest <= highest);

    // Unsigned, so that a range as wide as all of int64 does not overflow.
    const auto width = static_cast<std::uint64_t> (highest) - static_cast<std::uint64_t> (lowest) + 1;
    const auto number = engine();

    // A width of 0 is the whole range of 64 bits.
    const auto distance = width == 0 ? number : number % width;

    return static_cast<std::int64_t> (static_cast<std::uint64_t> (lowest) + distance);
}

int RandomSource::intBetween (int lowest, int highest)
{
    return static_cast<int> (between (lowest, highest));
}

bool RandomSource::chance (int percent)
{
    return between (1, 100) <= percent;
}

std::size_t RandomSource::indexBelow (std::size_t size)
{
    assert (size > 0);

    return static_cast<std::size_t> (engine() % size);
}

} // namespace guardrails
