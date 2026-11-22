#include "SeedRange.h"

#include <cstdlib>
#include <limits>

namespace guardrails
{

namespace
{

    const char* const countVariable = "HUMS_PROPERTY_SEEDS";
    const char* const firstSeedVariable = "HUMS_PROPERTY_FIRST_SEED";

    // Decimal digits and nothing else, and a value that fits.
    std::optional<std::uint64_t> wholeNumberFrom (const std::string& text)
    {
        if (text.empty())
            return std::nullopt;

        const auto largest = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t number = 0;

        for (const auto character : text)
        {
            if (character < '0' || character > '9')
                return std::nullopt;

            const auto digit = static_cast<std::uint64_t> (character - '0');

            if (number > (largest - digit) / 10)
                return std::nullopt;

            number = number * 10 + digit;
        }

        return number;
    }

} // namespace

std::optional<SeedRange> SeedRange::fromEnvironment (std::uint64_t countWhenNotSet)
{
    return fromTexts (std::getenv (countVariable), std::getenv (firstSeedVariable), countWhenNotSet);
}

std::optional<SeedRange> SeedRange::fromTexts (const char* countText, const char* firstSeedText, std::uint64_t countWhenNotSet)
{
    SeedRange range;
    range.count = countWhenNotSet;

    if (countText != nullptr)
    {
        const auto count = wholeNumberFrom (countText);

        if (! count.has_value())
            return std::nullopt;

        range.count = *count;
    }

    if (firstSeedText != nullptr)
    {
        const auto firstSeed = wholeNumberFrom (firstSeedText);

        if (! firstSeed.has_value())
            return std::nullopt;

        range.firstSeed = *firstSeed;
    }

    // The last seed is firstSeed + count - 1.
    if (range.count < 1 || range.count - 1 > std::numeric_limits<std::uint64_t>::max() - range.firstSeed)
        return std::nullopt;

    return range;
}

std::string SeedRange::howToRepeat (std::uint64_t seed)
{
    return std::string (firstSeedVariable) + "=" + std::to_string (seed) + " " + countVariable + "=1";
}

} // namespace guardrails
