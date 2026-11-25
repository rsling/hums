// guardrails::AllocationCounter: what the render checks count allocations
// with (docs/GUARDRAILS.md -> 4, Beyond the golden files). A counter that
// counts nothing would let every render pass for allocation-free, so it is
// shown here to see an allocation, in the configuration in which it counts.

#include <catch2/catch_test_macros.hpp>

#include "AllocationCounter.h"

#include <cstddef>
#include <memory>
#include <vector>

#if HUMS_COUNT_ALLOCATIONS

TEST_CASE ("guardrail 4: the allocation counter sees an allocation, and none where there is none", "[guardrails][AllocationCounter]")
{
    REQUIRE (guardrails::AllocationCounter::isCounting());

    std::vector<int> numbers;
    numbers.reserve (16);

    const std::size_t before = guardrails::AllocationCounter::count();

    // Inside what was reserved: no allocation.
    for (int number = 0; number < 16; ++number)
        numbers.push_back (number);

    CHECK (guardrails::AllocationCounter::count() == before);

    // One more than was reserved: the vector must grow.
    numbers.push_back (16);
    CHECK (guardrails::AllocationCounter::count() == before + 1);

    const auto single = std::make_unique<int> (1);
    const auto array = std::make_unique<int[]> (4);
    CHECK (guardrails::AllocationCounter::count() == before + 3);
}

#else

TEST_CASE ("guardrail 4: in an instrumented configuration the allocation counter counts nothing", "[guardrails][AllocationCounter]")
{
    CHECK (! guardrails::AllocationCounter::isCounting());

    const auto single = std::make_unique<int> (1);
    CHECK (guardrails::AllocationCounter::count() == 0);
}

#endif
