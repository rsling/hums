#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace guardrails
{

// What the property tests do with one seed (docs/GUARDRAILS.md -> 3): the
// document of the seed is built and held to the document checks, then a
// sequence of random commands is applied to it, each held to the command
// checks. The document is also rendered and held to the render checks
// (docs/GUARDRAILS.md -> 4, Beyond the golden files), twice: as it was
// built, and as the commands have left it.
//
// It stands here, and not in the test that goes through the seeds, so that
// the test which shows what the run reaches counts the commands of this very
// loop: a loop of its own beside it, with other random numbers, once showed
// every command to make a clone that the run itself did not make in the
// seeds of an instrumented configuration.
class PropertyRun
{
public:
    // What one command of a seed did.
    struct CommandOutcome
    {
        // The command's name as the driver's table has it.
        std::string name;

        bool wasApplied = false;

        // How many clips the pool held more after the command than before.
        // For a command that changes a clip, 1 says that the refcount gate
        // made a clone.
        int clipsAdded = 0;
    };

    // The findings of the first thing that fails, with what it was; empty
    // when the document and every command passed. With `outcomes`, every
    // command that passed its checks is added to it, in the order of the
    // run.
    static std::vector<std::string> failuresOfSeed (std::uint64_t seed, std::vector<CommandOutcome>* outcomes = nullptr);
};

} // namespace guardrails
