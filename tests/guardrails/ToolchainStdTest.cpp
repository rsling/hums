// Proves the C++20 standard library builds and runs in this configuration
// without JUCE. Render-core tests will have exactly this shape.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

TEST_CASE ("std-only test builds and runs", "[toolchain]")
{
    std::vector<std::int64_t> ticks { 960, 0, 480 };
    std::ranges::sort (ticks);

    REQUIRE (ticks == std::vector<std::int64_t> { 0, 480, 960 });
}
