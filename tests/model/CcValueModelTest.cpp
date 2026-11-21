// model::CcValueModel: one CC value of a Cell. docs/SCHEMA.md -> Step clip: Grid.

#include <catch2/catch_test_macros.hpp>

#include "model/CcValueModel.h"

TEST_CASE ("A CcValue gives back what is stored", "[model][CcValueModel]")
{
    const model::CcValueModel ccValue (juce::ValueTree::fromXml (R"(<CcValue cc="74" value="90"/>)"));

    CHECK (ccValue.getCc() == 74);
    CHECK (ccValue.getValue() == 90);
}
