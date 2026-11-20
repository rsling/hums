#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace miditiming
{

// DIN MIDI: 31 250 baud, 10 bits per byte.
constexpr std::int64_t dinNanosecondsPerByte = 320'000;

struct Stats
{
    std::size_t count = 0;
    double median = 0, p99 = 0, max = 0, min = 0; // milliseconds
};

// Nearest-rank median, p99 and max of nanosecond values, in milliseconds.
Stats statsOf (std::vector<std::int64_t> values);

// One aligned report line: label, median, p99, max, min and count.
std::string statsLine (const char* label, const Stats& s);

// Least-squares slope of y over x; 0 with fewer than two points.
double slopeOf (const std::vector<std::pair<double, double>>& points);

} // namespace miditiming
