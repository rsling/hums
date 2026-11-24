// Guardrail 3: invariants as property tests (docs/GUARDRAILS.md -> 3).
//
// For every seed of the run a random valid document is built and held to the
// document checks: the Validator, declared names only, and canonical save ->
// load -> save byte identity. Then a sequence of random commands of
// model/commands/ is applied to it, valid and invalid forms alike, and after
// each the document and the undo history are held to the command checks.
// What is done with one seed is guardrails::PropertyRun.

#include <catch2/catch_test_macros.hpp>

#include "PropertyRun.h"
#include "SeedRange.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{

// One line per finding.
std::string joined (const std::vector<std::string>& failures)
{
    std::string text;

    for (const auto& failure : failures)
        text += "\n  " + failure;

    return text;
}

} // namespace

TEST_CASE ("guardrail 3: every generated document, and every document a command leaves, is valid, uses declared names only and is the same after save and reload", "[guardrails][property]")
{
    const auto range = guardrails::SeedRange::fromEnvironment (HUMS_PROPERTY_SEEDS_DEFAULT);

    if (! range.has_value())
        FAIL ("HUMS_PROPERTY_SEEDS must be a whole number of 1 or more, HUMS_PROPERTY_FIRST_SEED one of 0 or more, and the seeds must not run past the largest number of 64 bits.");

    for (std::uint64_t done = 0; done < range->count; ++done)
    {
        const auto seed = range->firstSeed + done;
        const auto failures = guardrails::PropertyRun::failuresOfSeed (seed);

        // The first seed that fails ends the run: its findings are what has
        // to be read, not those of a thousand seeds after it.
        if (! failures.empty())
            FAIL ("seed " << seed << " failed (run it again alone with " << guardrails::SeedRange::howToRepeat (seed) << "):" << joined (failures));
    }

    SUCCEED ("seeds " << range->firstSeed << " to " << range->firstSeed + (range->count - 1));
}
