#include "TrafficPattern.h"

#include <cmath>

namespace miditiming
{

TrafficPattern::TrafficPattern (Density density, int portCount, double bpm, double minutes)
{
    const auto trackCount = density == Density::single ? 1 : 4;

    std::vector<int> trackPort (static_cast<std::size_t> (trackCount));
    for (int track = 0; track < trackCount; ++track)
        trackPort[static_cast<std::size_t> (track)] = density == Density::spread ? track % portCount : 0;

    const auto stepNanoseconds = static_cast<std::int64_t> (std::llround (60.0e9 / bpm / 4.0));
    const auto stepCount = static_cast<std::int64_t> (minutes * 60.0e9 / static_cast<double> (stepNanoseconds));
    length = stepCount * stepNanoseconds;

    pattern.reserve (static_cast<std::size_t> (stepCount * trackCount * 3));

    std::vector<int> pitch (static_cast<std::size_t> (trackCount));

    for (std::int64_t step = 0; step < stepCount; ++step)
    {
        const auto start = step * stepNanoseconds;
        const auto counter = static_cast<int> (step);

        for (int track = 0; track < trackCount; ++track)
        {
            const auto channel = static_cast<std::uint8_t> (track);
            const auto port = trackPort[static_cast<std::size_t> (track)];
            auto& p = pitch[static_cast<std::size_t> (track)];
            p = 36 + 12 * track + counter % 24;

            pattern.push_back ({ start, port, { static_cast<std::uint8_t> (0xb0 | channel), 74, static_cast<std::uint8_t> (counter % 128) }, 3 });
            pattern.push_back ({ start, port, { static_cast<std::uint8_t> (0x90 | channel), static_cast<std::uint8_t> (p), static_cast<std::uint8_t> (1 + counter % 127) }, 3 });
        }

        for (int track = 0; track < trackCount; ++track)
        {
            const auto channel = static_cast<std::uint8_t> (track);
            const auto port = trackPort[static_cast<std::size_t> (track)];
            const auto p = pitch[static_cast<std::size_t> (track)];

            pattern.push_back ({ start + stepNanoseconds / 2, port, { static_cast<std::uint8_t> (0x80 | channel), static_cast<std::uint8_t> (p), static_cast<std::uint8_t> (counter % 128) }, 3 });
        }
    }

    description = nameOf (density) + ": " + std::to_string (trackCount) + " track(s), sixteenths at "
                + std::to_string (static_cast<int> (bpm)) + " bpm, CC 74 + note-on per step, note-off at half step";

    if (density == Density::spread)
        description += ", tracks round-robin over " + std::to_string (portCount) + " port(s)";
}

Density densityFromName (const std::string& name, bool& ok)
{
    ok = true;
    if (name == "single") return Density::single;
    if (name == "stacked") return Density::stacked;
    if (name == "spread") return Density::spread;
    ok = false;
    return Density::single;
}

std::string nameOf (Density density)
{
    switch (density)
    {
        case Density::single: return "single";
        case Density::stacked: return "stacked";
        case Density::spread: return "spread";
    }
    return "?";
}

} // namespace miditiming
