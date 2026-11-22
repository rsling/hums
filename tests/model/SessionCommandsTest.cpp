// model::commands for the Session node: launch grid and scene names
// (docs/SCHEMA.md -> Session).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/SessionModel.h"
#include "model/commands/SessionCommands.h"

#include <cstdint>

namespace
{

namespace commands = model::commands;

model::SessionModel sessionOf (const CommandFixture& fixture)
{
    return model::SessionModel (fixture.session());
}

} // namespace

TEST_CASE ("setLaunchQuantise sets the launch grid, 0 for immediate", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setLaunchQuantise (fixture.project, 960, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (sessionOf (fixture).getLaunchQuantise() == 960);

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setLaunchQuantise (fixture.project, 0, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    CHECK (sessionOf (fixture).getLaunchQuantise() == 0);

    REQUIRE (fixture.undoManager.undo());
    CHECK (sessionOf (fixture).getLaunchQuantise() == 960);
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (sessionOf (fixture).getLaunchQuantise() == 3840);
}

TEST_CASE ("setLaunchQuantise refuses a negative grid", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setLaunchQuantise (fixture.project, -1, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("setSceneName renames a scene that has a node", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setSceneName (fixture.project, 2, "Refrain", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (sessionOf (fixture).getScenes().size() == 1);
    CHECK (sessionOf (fixture).getScenes()[0].getName() == "Refrain");

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("setSceneName gives a row its node when it gets its first name", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::setSceneName (fixture.project, 0, "Intro", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto scenes = sessionOf (fixture).getScenes();
    REQUIRE (scenes.size() == 2);
    CHECK (scenes[1].getIndex() == 0);
    CHECK (scenes[1].getName() == "Intro");

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("An empty scene name takes the name away and adds no node for it", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    // Row 5 has no node and so no name already.
    CHECK (commands::setSceneName (fixture.project, 5, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));

    REQUIRE (commands::setSceneName (fixture.project, 2, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
    REQUIRE (sessionOf (fixture).getScenes().size() == 1);
    CHECK (sessionOf (fixture).getScenes()[0].getIndex() == 2);
    CHECK (sessionOf (fixture).getScenes()[0].getName().isEmpty());
}

TEST_CASE ("setSceneName refuses a row before the first", "[model][commands][SessionCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setSceneName (fixture.project, -1, "x", fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}
