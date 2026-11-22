// guardrails::CommandDriver: the random commands of the property tests
// (docs/GUARDRAILS.md -> 3). A command the driver never reaches is a command
// the property tests do not hold to anything.

#include <catch2/catch_test_macros.hpp>

#include "CommandChecks.h"
#include "CommandDriver.h"
#include "DocumentGenerator.h"
#include "RandomSource.h"

#include <cstdint>
#include <set>
#include <string>

namespace
{

// The description of an attempt begins with the command's name.
std::string nameIn (const std::string& description)
{
    return description.substr (0, description.find (' '));
}

} // namespace

TEST_CASE ("guardrail 3: the driver's table of commands has no name twice", "[guardrails][CommandDriver]")
{
    std::set<std::string> names;

    for (const auto& command : guardrails::CommandDriver::knownCommands())
        CHECK (names.insert (command.name).second);

    CHECK (! names.empty());
}

TEST_CASE ("guardrail 3: over the first 100 seeds every command is applied in a valid form and refused in an invalid one", "[guardrails][CommandDriver]")
{
    std::set<std::string> applied;
    std::set<std::string> refused;

    for (std::uint64_t seed = 1; seed <= 100; ++seed)
    {
        auto project = guardrails::DocumentGenerator::generate (seed);
        juce::UndoManager undoManager;
        guardrails::RandomSource random (seed);
        guardrails::CommandDriver driver (project, undoManager, random);

        for (int command = 0; command < 25; ++command)
        {
            undoManager.beginNewTransaction();
            const auto attempt = driver.attemptOne();

            // What is expected of a form is what happened; that every form
            // is expected rightly is the property test's to show.
            INFO (attempt.description);
            REQUIRE (attempt.expectedToApply == attempt.wasApplied);
            (attempt.wasApplied ? applied : refused).insert (nameIn (attempt.description));
        }
    }

    for (const auto& command : guardrails::CommandDriver::knownCommands())
    {
        INFO (command.name);
        CHECK (applied.contains (command.name));
        CHECK (refused.contains (command.name) == command.hasInvalidForm);
    }
}

TEST_CASE ("guardrail 3: the same seed gives the same commands", "[guardrails][CommandDriver]")
{
    // Compared by their names: an id that a command made is random, so the
    // arguments of a later command that names it are not the same twice.
    const auto namesOfRun = [] {
        auto project = guardrails::DocumentGenerator::generate (7);
        juce::UndoManager undoManager;
        guardrails::RandomSource random (7);
        guardrails::CommandDriver driver (project, undoManager, random);
        std::string names;

        for (int command = 0; command < 40; ++command)
        {
            const auto attempt = driver.attemptOne();
            names += nameIn (attempt.description) + (attempt.wasApplied ? "+ " : "- ");
        }

        return names;
    };

    CHECK (namesOfRun() == namesOfRun());
}
