#pragma once

#include <cstddef>

namespace guardrails
{

// Counts what a test program takes from the heap with `new`, so that a test
// can show that a stretch of code allocates nothing (docs/GUARDRAILS.md -> 4.
// Render golden files, Beyond the golden files): the count is read before and
// after, and the two must be the same.
//
// It counts by replacing the global operator new of the program it is
// compiled into (hums_count_allocations in tests/CMakeLists.txt). In the
// instrumented configurations nothing is replaced and nothing is counted:
// there the sanitizer has its own operator new, with which it checks that
// what `new` gave goes back through the matching `delete`, and a replacement
// would take that check away. Whether a piece of code allocates does not
// depend on the configuration, and the gate always runs the plain one.
//
// What is counted is `new` in all its forms, which is what the standard
// library's containers and strings allocate with. A direct call of malloc is
// not seen.
//
// Std only, so that the render-core tests can count too.
class AllocationCounter
{
public:
    // Whether this program counts: true in the plain configuration.
    static bool isCounting() noexcept;

    // How many allocations the program has made so far, on any thread; 0
    // where nothing is counted.
    static std::size_t count() noexcept;
};

} // namespace guardrails
