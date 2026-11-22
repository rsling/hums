// io::Migrations: the table of steps between schema versions,
// docs/SCHEMA.md -> Migrations. How the load path runs the steps is tested
// with io::ProjectFile.

#include <catch2/catch_test_macros.hpp>

#include "io/Migrations.h"
#include "model/ProjectModel.h"

TEST_CASE ("The schema is at version 1 and has no migration step", "[io][Migrations]")
{
    // The first format change adds its step here, and with it a fixture file
    // of version 1 and a test that loads it.
    CHECK (io::Migrations::steps().empty());
    CHECK (io::Migrations::currentVersion() == 1);
}

TEST_CASE ("Every step raises the version by one, from 1", "[io][Migrations]")
{
    const io::Migrations::Step nothing = [] (juce::ValueTree&) {};

    CHECK (io::Migrations::versionReachedBy ({}) == 1);
    CHECK (io::Migrations::versionReachedBy ({ nothing }) == 2);
    CHECK (io::Migrations::versionReachedBy ({ nothing, nothing, nothing }) == 4);
}

TEST_CASE ("The fixtures of valid projects are of the current version", "[io][Migrations]")
{
    for (const auto* fileName : { "all_node_types.xml", "berlin_test.xml", "berlin_test_canonical.xml" })
    {
        INFO (fileName);
        const auto file = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile (fileName);
        const auto project = juce::ValueTree::fromXml (file.loadFileAsString());
        REQUIRE (project.isValid());

        CHECK (model::ProjectModel (project).getSchemaVersion() == io::Migrations::currentVersion());
    }
}
