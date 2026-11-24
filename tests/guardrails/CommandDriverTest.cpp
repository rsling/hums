// guardrails::CommandDriver: the random commands of the property tests
// (docs/GUARDRAILS.md -> 3). A command the driver never reaches is a command
// the property tests do not hold to anything.

#include <catch2/catch_test_macros.hpp>

#include "CommandChecks.h"
#include "CommandDriver.h"
#include "DocumentGenerator.h"
#include "PropertyRun.h"
#include "RandomSource.h"
#include "model/Ids.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace
{

// The description of an attempt begins with the command's name.
std::string nameIn (const std::string& description)
{
    return description.substr (0, description.find (' '));
}

// Makes every placement under `node` one tick long. The document stays
// valid: no placement reaches further than it did.
void shortenEveryPlacement (juce::ValueTree node)
{
    if (node.hasType (model::ids::Placement))
        node.setProperty (model::ids::length, static_cast<juce::int64> (1), nullptr);

    for (auto child : node)
        shortenEveryPlacement (child);
}

} // namespace

TEST_CASE ("guardrail 3: the driver's table of commands has no name twice", "[guardrails][CommandDriver]")
{
    std::set<std::string> names;

    for (const auto& command : guardrails::CommandDriver::knownCommands())
        CHECK (names.insert (command.name).second);

    CHECK (! names.empty());
}

TEST_CASE ("guardrail 3: over the first 300 seeds of the property run every command is applied in a valid form and refused in an invalid one", "[guardrails][CommandDriver]")
{
    std::set<std::string> applied;
    std::set<std::string> refused;

    // The commands that change a clip, counted when the pool was one clip
    // larger afterwards: the refcount gate made a clone.
    std::set<std::string> madeUnique;

    // The seeds and the commands are those of the property run itself
    // (PropertyRun), so what is counted here is what that run does. 300 in
    // every configuration, although an instrumented one goes through 200
    // seeds by default: removeLane, moveLane and the like need a shared step
    // clip that has lanes, and the last command to make its first clone
    // makes it at seed 220. The instrumented configurations get the clones
    // of the seeds past 200 from this test.
    for (std::uint64_t seed = 1; seed <= 300; ++seed)
    {
        std::vector<guardrails::PropertyRun::CommandOutcome> outcomes;
        const auto failures = guardrails::PropertyRun::failuresOfSeed (seed, &outcomes);

        INFO ("seed " << seed);
        REQUIRE (failures.empty());

        for (const auto& outcome : outcomes)
        {
            if (outcome.clipsAdded == 1)
                madeUnique.insert (outcome.name);

            (outcome.wasApplied ? applied : refused).insert (outcome.name);
        }
    }

    for (const auto& command : guardrails::CommandDriver::knownCommands())
    {
        INFO (command.name);
        CHECK (applied.contains (command.name));
        CHECK (refused.contains (command.name) == command.hasInvalidForm);
    }

    // A gate that never clones in the property tests is held to nothing.
    CHECK (madeUnique.contains ("setClipName"));
    CHECK (madeUnique.contains ("setClipColour"));

    for (const auto* command : { "addNote", "setNote", "removeNote", "addEvent", "setEvent", "removeEvent", "setClipLoopLength", "setClipQuantise", "addLane", "removeLane", "moveLane", "setLaneName", "setLaneFixedPitch", "setCell", "setCellCc",
                                 "removeCellCc", "setStepLength", "setStepCount", "setResetAt", "setColumnSkipped" })
    {
        INFO (command);
        CHECK (madeUnique.contains (command));
    }
}

TEST_CASE ("guardrail 3: a start the driver takes for impossible is one that even a placement of one tick cannot have", "[guardrails][CommandDriver]")
{
    // A placement of one tick fits at the tick before the largest, and one
    // of three ticks three before it: a start is impossible for a length,
    // not in itself. The driver once handed out the last four ticks for any
    // length, and the property test then called a correct move a failure
    // (seed 83189).
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        auto project = guardrails::DocumentGenerator::generate (seed);

        juce::UndoManager undoManager;
        guardrails::RandomSource random (seed);
        guardrails::CommandDriver driver (project, undoManager, random);

        for (int command = 0; command < 250; ++command)
        {
            // Before every command, so that a placement the last one added
            // or resized is short too. Nothing is undone in this test, so
            // the undo history need not know of it.
            shortenEveryPlacement (project);

            undoManager.beginNewTransaction();
            const auto attempt = driver.attemptOne();

            INFO ("seed " << seed << ": " << attempt.description);
            REQUIRE (attempt.expectedToApply == attempt.wasApplied);
        }
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
