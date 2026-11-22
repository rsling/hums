// guardrails::CommandChecks: what the property tests hold of every command
// (docs/GUARDRAILS.md -> 3). Each check is shown to find the fault it is
// there to find, in a "command" written here to have exactly that fault.

#include <catch2/catch_test_macros.hpp>

#include "CommandChecks.h"
#include "DocumentChecks.h"
#include "DocumentGenerator.h"
#include "model/Ids.h"
#include "model/commands/RoutingCommands.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
using guardrails::CommandChecks;

bool anyContains (const std::vector<std::string>& failures, const std::string& text)
{
    for (const auto& failure : failures)
        if (failure.find (text) != std::string::npos)
            return true;

    return false;
}

// A clip that some Placement or Slot references; invalid when there is none.
juce::ValueTree referencedClipOf (const juce::ValueTree& project, const juce::ValueTree& node)
{
    if (node.hasProperty (ids::clipId))
        return project.getChildWithName (ids::ClipPool).getChildWithProperty (ids::id, node[ids::clipId].toString());

    for (const auto& child : node)
        if (const auto clip = referencedClipOf (project, child); clip.isValid())
            return clip;

    return {};
}

// A generated document in which a clip is referenced, which the clip-kind
// check needs: that of the first seed that gives one.
juce::ValueTree validDocument()
{
    for (std::uint64_t seed = 1;; ++seed)
    {
        const auto project = guardrails::DocumentGenerator::generate (seed);

        if (! referencedClipOf (project, project.getChildWithName (ids::Tracks)).isValid())
            continue;

        REQUIRE (guardrails::DocumentChecks::failuresOf (project).empty());

        return project;
    }
}

const juce::Identifier inventedProperty ("swing");

} // namespace

TEST_CASE ("guardrail 3: a real command, applied and refused, passes the command checks", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto applied = CommandChecks::failuresOf (project, undoManager, [&] {
        const auto result = model::commands::addPlayer (project, "Minimoog", undoManager);
        return CommandChecks::Attempt { "addPlayer", true, result.wasApplied() };
    });

    CHECK (applied.empty());
    // Still applied after the checks' undo and redo.
    CHECK (undoManager.canUndo());
    CHECK (! undoManager.canRedo());

    const auto refused = CommandChecks::failuresOf (project, undoManager, [&] {
        const auto result = model::commands::removePlayer (project, "no such id", undoManager);
        return CommandChecks::Attempt { "removePlayer", false, result.wasApplied() };
    });

    CHECK (refused.empty());
}

TEST_CASE ("guardrail 3: a command that leaves an invalid document is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::ppq, 480, &undoManager);
        return CommandChecks::Attempt { "setPpq 480", true, true };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "setPpq 480: the document after it: invalid: Project, ppq"));
}

TEST_CASE ("guardrail 3: a command that writes an invented name is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.getChildWithName (ids::Session).setProperty (inventedProperty, 60, &undoManager);
        return CommandChecks::Attempt { "setSwing", true, true };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "undeclared property \"swing\""));
}

TEST_CASE ("guardrail 3: a valid command that is refused, and an invalid one that is applied, are found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto refused = CommandChecks::failuresOf (project, undoManager, [] {
        return CommandChecks::Attempt { "refuses", true, false };
    });

    REQUIRE (refused.size() == 1);
    CHECK (anyContains (refused, "refuses: a valid command was refused"));

    const auto applied = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::name, "applied", &undoManager);
        return CommandChecks::Attempt { "applies", false, true };
    });

    REQUIRE (applied.size() == 1);
    CHECK (anyContains (applied, "applies: an invalid command was applied"));
}

TEST_CASE ("guardrail 3: a refused command that has changed the document is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    // Half-applied: one write, then the refusal.
    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::name, "half", nullptr);
        return CommandChecks::Attempt { "half-applies", false, false };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "it was refused and changed the document all the same"));
}

TEST_CASE ("guardrail 3: a refused command that has changed the undo history is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    // It takes its write back before it refuses, and the document is as it
    // was; but the history has two actions more, and the redo history is
    // gone.
    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        const auto nameBefore = project[ids::name];
        project.setProperty (ids::name, "tried", &undoManager);
        project.setProperty (ids::name, nameBefore, &undoManager);
        return CommandChecks::Attempt { "tries and reverts", false, false };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "it was refused and changed the undo history all the same"));
}

TEST_CASE ("guardrail 3: a refused command that has emptied the undo history is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    project.setProperty (ids::name, "first", &undoManager);
    REQUIRE (undoManager.canUndo());

    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        undoManager.clearUndoHistory();
        return CommandChecks::Attempt { "forgets", false, false };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "it was refused and changed the undo history all the same"));
}

TEST_CASE ("guardrail 3: a command that begins a transaction of its own is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    // One undo then takes back only what came after the command's own
    // transaction began.
    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::name, "one", &undoManager);
        undoManager.beginNewTransaction();
        project.getChildWithName (ids::Session).setProperty (ids::launchQuantise, 123, &undoManager);
        return CommandChecks::Attempt { "two transactions", true, true };
    });

    CHECK (anyContains (failures, "two transactions: undo did not bring back the document as it was before"));
}

TEST_CASE ("guardrail 3: a command that undo does not take back is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    // One write the history knows and one it does not.
    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::name, "known", &undoManager);
        project.getChildWithName (ids::Session).setProperty (ids::launchQuantise, 123, nullptr);
        return CommandChecks::Attempt { "writes past undo", true, true };
    });

    CHECK (anyContains (failures, "writes past undo: undo did not bring back the document as it was before"));
}

TEST_CASE ("guardrail 3: a command that changes the document with nothing in the undo history is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.setProperty (ids::name, "unknown to undo", nullptr);
        return CommandChecks::Attempt { "writes without undo", true, true };
    });

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "it changed the document without the undo history knowing"));
}

TEST_CASE ("guardrail 3: a change to UiState is no change to the undoable subtree", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    const auto failures = CommandChecks::failuresOf (project, undoManager, [&] {
        project.getChildWithName (ids::UiState).setProperty (ids::activeTab, "routing", nullptr);
        return CommandChecks::Attempt { "a setting beside a refusal", false, false };
    });

    CHECK (failures.empty());
}

TEST_CASE ("guardrail 3: a command that changes the kind of a referenced clip, or removes the clip, is found", "[guardrails][CommandChecks]")
{
    auto project = validDocument();
    juce::UndoManager undoManager;

    auto clip = referencedClipOf (project, project.getChildWithName (ids::Tracks));
    REQUIRE (clip.isValid());

    // Only the clip-kind check is looked at: such a document is invalid in
    // other ways too.
    const auto otherKind = clip[ids::kind].toString() == "step" ? "event" : "step";

    const auto changed = CommandChecks::failuresOf (project, undoManager, [&] {
        clip.setProperty (ids::kind, otherKind, &undoManager);
        return CommandChecks::Attempt { "changes the kind", true, true };
    });

    CHECK (anyContains (changed, "changes the kind: the referenced clip " + clip[ids::id].toString().toStdString() + " was of kind"));
    REQUIRE (undoManager.undo());

    const auto removed = CommandChecks::failuresOf (project, undoManager, [&] {
        clip.getParent().removeChild (clip, &undoManager);
        return CommandChecks::Attempt { "removes the clip", true, true };
    });

    CHECK (anyContains (removed, "removes the clip: the referenced clip " + clip[ids::id].toString().toStdString() + " is gone"));
}
