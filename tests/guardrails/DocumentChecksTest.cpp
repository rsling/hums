// guardrails::DocumentChecks: what the property tests hold of every document
// (docs/GUARDRAILS.md -> 3). Each check is shown to find what it is there to
// find; a check that finds nothing in anything would let every seed pass.

#include <catch2/catch_test_macros.hpp>

#include "DocumentChecks.h"
#include "DocumentGenerator.h"
#include "model/Ids.h"

#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;

// Any valid document will do as the ground the faults are planted in.
juce::ValueTree validDocument()
{
    const auto project = guardrails::DocumentGenerator::generate (1);
    REQUIRE (guardrails::DocumentChecks::failuresOf (project).empty());

    return project;
}

bool anyContains (const std::vector<std::string>& failures, const std::string& text)
{
    for (const auto& failure : failures)
        if (failure.find (text) != std::string::npos)
            return true;

    return false;
}

// Names that model/Ids.h does not declare. Spelled as strings on purpose:
// this is the one thing the check is there to find.
const juce::Identifier inventedProperty ("swing");
const juce::Identifier inventedType ("Groove");

} // namespace

TEST_CASE ("guardrail 3: a document that breaks an invariant is found", "[guardrails][DocumentChecks]")
{
    auto project = validDocument();
    project.setProperty (ids::ppq, 480, nullptr);

    const auto failures = guardrails::DocumentChecks::failuresOf (project);

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "invalid: Project, ppq"));
    CHECK (guardrails::DocumentChecks::invariantFailuresOf (project) == failures);
}

TEST_CASE ("guardrail 3: an invented property name in the undoable subtree is found", "[guardrails][DocumentChecks]")
{
    auto project = validDocument();
    project.getChildWithName (ids::Tracks).setProperty (inventedProperty, 60, nullptr);

    // The Validator passes over what it does not know, so that a file from a
    // newer build can be loaded; only this check sees the name [D15].
    CHECK (guardrails::DocumentChecks::invariantFailuresOf (project).empty());

    const auto failures = guardrails::DocumentChecks::failuresOf (project);

    REQUIRE (failures.size() == 1);
    CHECK (failures.front() == "undeclared property \"swing\" at Project/Tracks");
}

TEST_CASE ("guardrail 3: an invented property name on the Project itself is found", "[guardrails][DocumentChecks]")
{
    auto project = validDocument();
    project.setProperty (inventedProperty, 60, nullptr);

    const auto failures = guardrails::DocumentChecks::undeclaredNamesIn (project);

    REQUIRE (failures.size() == 1);
    CHECK (failures.front() == "undeclared property \"swing\" at Project");
}

TEST_CASE ("guardrail 3: an invented node type in the undoable subtree is found, with what it holds", "[guardrails][DocumentChecks]")
{
    auto project = validDocument();

    juce::ValueTree invented (inventedType);
    invented.setProperty (ids::name, "declared, and no finding", nullptr);
    invented.setProperty (inventedProperty, 60, nullptr);
    project.getChildWithName (ids::ClipPool).appendChild (invented, nullptr);

    const auto failures = guardrails::DocumentChecks::failuresOf (project);

    REQUIRE (failures.size() == 2);
    CHECK (failures[0] == "undeclared node type \"Groove\" at Project/ClipPool/Groove");
    CHECK (failures[1] == "undeclared property \"swing\" at Project/ClipPool/Groove");
}

TEST_CASE ("guardrail 3: names under UiState are not looked at", "[guardrails][DocumentChecks]")
{
    // UiState is not part of the undoable subtree, and no command writes it
    // (docs/GUARDRAILS.md -> 3).
    auto project = validDocument();
    auto uiState = project.getChildWithName (ids::UiState);

    uiState.setProperty (inventedProperty, 60, nullptr);
    uiState.appendChild (juce::ValueTree (inventedType), nullptr);

    CHECK (guardrails::DocumentChecks::undeclaredNamesIn (project).empty());
    CHECK (guardrails::DocumentChecks::failuresOf (project).empty());
}

TEST_CASE ("guardrail 3: a document that cannot be loaded again after a save is found", "[guardrails][DocumentChecks]")
{
    // A schemaVersion from a newer build is no violation of an invariant,
    // but the load path refuses the file.
    auto project = validDocument();
    project.setProperty (ids::schemaVersion, 1000, nullptr);

    CHECK (guardrails::DocumentChecks::invariantFailuresOf (project).empty());

    const auto failures = guardrails::DocumentChecks::failuresOf (project);

    REQUIRE (failures.size() == 1);
    CHECK (anyContains (failures, "the saved document was not loaded again"));
    CHECK (guardrails::DocumentChecks::saveLoadSaveFailuresOf (project) == failures);
}

TEST_CASE ("guardrail 3: the checks leave the document as it was", "[guardrails][DocumentChecks]")
{
    const auto project = validDocument();
    const auto before = project.createCopy();

    guardrails::DocumentChecks::failuresOf (project);

    CHECK (project.isEquivalentTo (before));
}
