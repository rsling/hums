#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace miditiming
{

// The three traffic densities of docs/ARCHITECTURE.md -> Timing.
enum class Density
{
    single,  // one track: one note at a time, plus its CC
    stacked, // four tracks on one port: four CCs and four note-ons at one tick
    spread   // the same four tracks spread over the given ports, round-robin
};

struct PatternMessage
{
    std::int64_t offsetNanoseconds = 0; // intended time, from the run's start
    int port = 0;                       // index into the --out/--in pairs
    std::array<std::uint8_t, 3> bytes {};
    std::uint8_t size = 0;
};

// Dense sixteenths with CC traffic, generated in full before the run so the
// sender never allocates. Per track and step: CC 74 then note-on at the step
// start, note-off half a step later. Each track has its own channel, and no
// two consecutive messages on one port share a status byte, so an interface
// can't use running status and the computed DIN wire time is exact. Data
// bytes count up per track, so a dropped message shows up in matching.
class TrafficPattern
{
public:
    TrafficPattern (Density density, int portCount, double bpm, double minutes);

    const std::vector<PatternMessage>& messages() const { return pattern; }
    std::int64_t lengthNanoseconds() const { return length; }
    std::string describe() const { return description; }

private:
    std::vector<PatternMessage> pattern;
    std::int64_t length = 0;
    std::string description;
};

Density densityFromName (const std::string& name, bool& ok);
std::string nameOf (Density density);

} // namespace miditiming
