#include "platform/mac/DeviceIdentifier.h"

#include <algorithm>
#include <charconv>

namespace platform
{

namespace
{
    // The last number of one item, or nullopt if the item is not one or two
    // whole decimal numbers separated by spaces.
    std::optional<std::int32_t> lastNumberOf (std::string_view item)
    {
        std::optional<std::int32_t> last;
        int numbers = 0;
        std::size_t i = 0;

        while (i < item.size())
        {
            if (item[i] == ' ')
            {
                ++i;
                continue;
            }

            const auto begin = i;
            while (i < item.size() && item[i] != ' ')
                ++i;

            std::int32_t value = 0;
            const auto* first = item.data() + begin;
            const auto* end = item.data() + i;
            const auto parsed = std::from_chars (first, end, value);

            // from_chars stops at the first character that doesn't belong, so
            // "12abc" has to be refused here; a number too large for a unique
            // id comes back as an error.
            if (parsed.ec != std::errc() || parsed.ptr != end)
                return std::nullopt;

            last = value;
            ++numbers;
        }

        if (numbers < 1 || numbers > 2)
            return std::nullopt;

        return last;
    }
} // namespace

std::optional<DeviceIdentifier> DeviceIdentifier::parse (std::string_view text)
{
    DeviceIdentifier result;

    while (true)
    {
        const auto comma = text.find (',');
        const auto id = lastNumberOf (text.substr (0, comma));

        if (! id)
            return std::nullopt;

        result.itemIds.push_back (*id);

        if (comma == std::string_view::npos)
            return result;

        text.remove_prefix (comma + 1);
    }
}

bool DeviceIdentifier::names (std::int32_t endpointId, std::span<const std::int32_t> connectionIds) const
{
    if (connectionIds.empty())
        return itemIds.size() == 1 && itemIds.front() == endpointId;

    return std::equal (itemIds.begin(), itemIds.end(), connectionIds.begin(), connectionIds.end());
}

} // namespace platform
