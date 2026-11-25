#include "PropertyRun.h"

#include "CommandChecks.h"
#include "CommandDriver.h"
#include "DocumentChecks.h"
#include "DocumentGenerator.h"
#include "RandomSource.h"
#include "RenderChecks.h"
#include "RenderRun.h"
#include "model/Ids.h"
#include "model/ProjectModel.h"
#include "render/SnapshotBuilder.h"

namespace guardrails
{

namespace
{

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

    // The description of an attempt begins with the command's name.
    std::string nameIn (const std::string& description)
    {
        return description.substr (0, description.find (' '));
    }

    // The document as the renderer plays it, held to the render checks
    // (docs/GUARDRAILS.md -> 4, Beyond the golden files). The destinations
    // are those without hardware, as in an export. The document must be
    // valid: the document checks have passed it.
    std::vector<std::string> renderFailuresOf (const juce::ValueTree& project)
    {
        const model::ProjectModel projectModel (project);
        const auto snapshot = render::SnapshotBuilder::build (projectModel);
        const auto destinations = render::SnapshotBuilder::destinationsWithoutHardware (projectModel);

        return RenderChecks::failuresOf (RenderRun (snapshot, destinations.destinationOfTrack, destinations.count));
    }

} // namespace

std::vector<std::string> PropertyRun::failuresOfSeed (std::uint64_t seed, std::vector<CommandOutcome>* outcomes)
{
    auto project = DocumentGenerator::generate (seed);
    auto failures = DocumentChecks::failuresOf (project);

    if (! failures.empty())
        return failures;

    failures = renderFailuresOf (project);

    if (! failures.empty())
        return failures;

    juce::UndoManager undoManager;
    RandomSource random (seed + commandSeedOffset);
    CommandDriver driver (project, undoManager, random);

    for (int command = 1; command <= commandsPerSeed; ++command)
    {
        const auto clipPool = project.getChildWithName (model::ids::ClipPool);
        const auto clipsBefore = clipPool.getNumChildren();
        CommandChecks::Attempt attempt;

        failures = CommandChecks::failuresOf (project, undoManager, [&driver, &attempt] {
            attempt = driver.attemptOne();
            return attempt;
        });

        if (! failures.empty())
        {
            failures.insert (failures.begin(), "at command " + std::to_string (command) + " of the seed:");
            return failures;
        }

        if (outcomes != nullptr)
            outcomes->push_back ({ nameIn (attempt.description), attempt.wasApplied, clipPool.getNumChildren() - clipsBefore });

        // Now and then the last change is taken back and left so: the next
        // command then meets a history with something to redo, which a
        // refused command must leave alone and an applied one replaces.
        if (random.chance (15) && undoManager.canUndo())
            undoManager.undo();
    }

    // Once more as the commands have left the document: editing makes
    // documents that the generator does not. Every command has left it valid.
    failures = renderFailuresOf (project);

    if (! failures.empty())
        failures.insert (failures.begin(), "after the commands of the seed:");

    return failures;
}

} // namespace guardrails
