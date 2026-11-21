#include "Statistics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace miditiming
{

Stats statsOf (std::vector<std::int64_t> values)
{
    Stats s;
    s.count = values.size();
    if (values.empty())
        return s;

    std::sort (values.begin(), values.end());
    const auto at = [&] (double p) {
        const auto rank = static_cast<std::size_t> (std::ceil (p * static_cast<double> (values.size())));
        return static_cast<double> (values[std::max<std::size_t> (rank, 1) - 1]) / 1.0e6;
    };

    s.median = at (0.5);
    s.p99 = at (0.99);
    s.max = static_cast<double> (values.back()) / 1.0e6;
    s.min = static_cast<double> (values.front()) / 1.0e6;
    return s;
}

std::string statsLine (const char* label, const Stats& s)
{
    char buffer[200];
    std::snprintf (buffer, sizeof (buffer), "%-34s median %8.3f ms   p99 %8.3f ms   max %8.3f ms   (min %.3f, n = %zu)",
                   label, s.median, s.p99, s.max, s.min, s.count);
    return buffer;
}

double slopeOf (const std::vector<std::pair<double, double>>& points)
{
    if (points.size() < 2)
        return 0;

    double mx = 0, my = 0;
    for (const auto& [x, y] : points)
    {
        mx += x;
        my += y;
    }
    mx /= static_cast<double> (points.size());
    my /= static_cast<double> (points.size());

    double sxy = 0, sxx = 0;
    for (const auto& [x, y] : points)
    {
        sxy += (x - mx) * (y - my);
        sxx += (x - mx) * (x - mx);
    }
    return sxx > 0 ? sxy / sxx : 0;
}

} // namespace miditiming
