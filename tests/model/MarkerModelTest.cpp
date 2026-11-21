// model::MarkerModel: a named position on the Timeline. docs/SCHEMA.md -> Timeline.

#include <catch2/catch_test_macros.hpp>

#include "model/MarkerModel.h"

TEST_CASE ("A Marker gives back what is stored", "[model][MarkerModel]")
{
    const model::MarkerModel marker (juce::ValueTree::fromXml (R"(<Marker tick="15360" name="Break"/>)"));

    CHECK (marker.getTick() == 15360);
    CHECK (marker.getName() == "Break");
}
