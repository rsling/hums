// Guardrail 3: invariants as property tests (docs/GUARDRAILS.md -> 3).
//
// For every seed of the run a random valid document is built and held to the
// document checks: the Validator, declared names only, and canonical save ->
// load -> save byte identity. Then a sequence of random commands of
// model/commands/ is applied to it, valid and invalid forms alike, and after
// each the document and the undo history are held to the command checks.

#include <catch2/catch_test_macros.hpp>

#include "CommandChecks.h"
#include "CommandDriver.h"
#include "DocumentChecks.h"
#include "DocumentGenerator.h"
#include "RandomSource.h"
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

// How many commands one document gets. Long enough for commands to meet what
// earlier ones have left (a port that was added is renamed, then removed);
// short, because every command that changes the document costs about as
// much as generating and checking the document did, and breadth comes from
// the seeds.
const int commandsPerSeed = 12;

// The commands of a seed are drawn from numbers of their own, so that a
// change to the generator leaves the commands of a seed as they were, and
// the other way round.
const std::uint64_t commandSeedOffset = 0x9e3779b97f4a7c15ULL;

// The document of one seed and then its commands; the findings of the first
// thing that fails, with what it was.
std::vector<std::string> failuresOfSeed (std::uint64_t seed)
{
    auto project = guardrails::DocumentGenerator::generate (seed);
    auto failures = guardrails::DocumentChecks::failuresOf (project);

    if (! failures.empty())
        return failures;

    juce::UndoManager undoManager;
    guardrails::RandomSource random (seed + commandSeedOffset);
    guardrails::CommandDriver driver (project, undoManager, random);

    for (int command = 1; command <= commandsPerSeed; ++command)
    {
        failures = guardrails::CommandChecks::failuresOf (project, undoManager, [&driver] { return driver.attemptOne(); });

        if (! failures.empty())
        {
            failures.insert (failures.begin(), "at command " + std::to_string (command) + " of the seed:");
            return failures;
        }

        // Now and then the last change is taken back and left so: the next
        // command then meets a history with something to redo, which a
        // refused command must leave alone and an applied one replaces.
        if (random.chance (15) && undoManager.canUndo())
            undoManager.undo();
    }

    return {};
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
        const auto failures = failuresOfSeed (seed);

        // The first seed that fails ends the run: its findings are what has
        // to be read, not those of a thousand seeds after it.
        if (! failures.empty())
            FAIL ("seed " << seed << " failed (run it again alone with " << guardrails::SeedRange::howToRepeat (seed) << "):" << joined (failures));
    }

    SUCCEED ("seeds " << range->firstSeed << " to " << range->firstSeed + (range->count - 1));
}
