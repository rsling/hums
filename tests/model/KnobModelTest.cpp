// model::KnobModel: one knob assignment of a Track. docs/SCHEMA.md -> Knobs / Knob.

#include <catch2/catch_test_macros.hpp>

#include "model/KnobModel.h"

TEST_CASE ("A Knob gives back what is stored", "[model][KnobModel]")
{
    const model::KnobModel knob (juce::ValueTree::fromXml (R"(<Knob cc="74" label="Cutoff"/>)"));

    CHECK (knob.getCc() == 74);
    CHECK (knob.getLabel() == "Cutoff");
}

TEST_CASE ("A Knob without a label has an empty one", "[model][KnobModel]")
{
    const model::KnobModel knob (juce::ValueTree::fromXml (R"(<Knob cc="0"/>)"));

    CHECK (knob.getCc() == 0);
    CHECK (knob.getLabel() == "");
}
