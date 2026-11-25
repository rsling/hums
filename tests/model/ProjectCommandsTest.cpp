// model::commands for the Project node: the project's name
// (docs/SCHEMA.md -> Project).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/Ids.h"
#include "model/ProjectModel.h"
#include "model/commands/ProjectCommands.h"

namespace
{

namespace commands = model::commands;

juce::String nameOf (const CommandFixture& fixture)
{
    return model::ProjectModel (fixture.project).getName();
}

} // namespace

TEST_CASE ("setProjectName renames the project", "[model][commands][ProjectCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setProjectName (fixture.project, "Berlin test", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (nameOf (fixture) == "Berlin test");

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (nameOf (fixture) == "Command test");

    REQUIRE (fixture.undoManager.redo());
    CHECK (nameOf (fixture) == "Berlin test");
}

TEST_CASE ("setProjectName gives a project without a name its first one, and undo takes the property away again", "[model][commands][ProjectCommands]")
{
    CommandFixture fixture;
    fixture.project.removeProperty (model::ids::name, nullptr);
    REQUIRE (fixture.isValid());
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setProjectName (fixture.project, "First", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (nameOf (fixture) == "First");

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.project.hasProperty (model::ids::name));
}

TEST_CASE ("setProjectName takes an empty name: the project then has none", "[model][commands][ProjectCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setProjectName (fixture.project, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (nameOf (fixture).isEmpty());

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setProjectName with the name the project has changes nothing and leaves nothing to undo", "[model][commands][ProjectCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    REQUIRE (commands::setProjectName (fixture.project, "Command test", fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setProjectName with an empty name stores nothing in a project that has no name", "[model][commands][ProjectCommands]")
{
    CommandFixture fixture;
    fixture.project.removeProperty (model::ids::name, nullptr);
    const auto before = fixture.xml();

    REQUIRE (commands::setProjectName (fixture.project, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}
