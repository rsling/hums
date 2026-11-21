// model::PadModel: one pad of the PadMap. docs/SCHEMA.md -> Routing.

#include <catch2/catch_test_macros.hpp>

#include "model/PadModel.h"

TEST_CASE ("A Pad gives back what is stored", "[model][PadModel]")
{
    const model::PadModel pad (juce::ValueTree::fromXml (
        R"(<Pad index="1" inputPitch="37" pitch="42" label="Hat"/>)"));

    CHECK (pad.getIndex() == 1);
    CHECK (pad.getInputPitch() == 37);
    CHECK (pad.getPitch() == 42);
    CHECK (pad.getLabel() == "Hat");
}

TEST_CASE ("A Pad without a label has an empty one", "[model][PadModel]")
{
    const model::PadModel pad (juce::ValueTree::fromXml (
        R"(<Pad index="0" inputPitch="36" pitch="36"/>)"));

    CHECK (pad.getLabel() == "");
}
