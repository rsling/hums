// model::SceneModel: the name of one session row. docs/SCHEMA.md -> Session.

#include <catch2/catch_test_macros.hpp>

#include "model/SceneModel.h"

TEST_CASE ("A Scene gives back what is stored", "[model][SceneModel]")
{
    const model::SceneModel scene (juce::ValueTree::fromXml (R"(<Scene index="2" name="Chorus"/>)"));

    CHECK (scene.getIndex() == 2);
    CHECK (scene.getName() == "Chorus");
}

TEST_CASE ("A Scene without a name has an empty one", "[model][SceneModel]")
{
    const model::SceneModel scene (juce::ValueTree::fromXml (R"(<Scene index="2"/>)"));

    CHECK (scene.getIndex() == 2);
    CHECK (scene.getName() == "");
}
