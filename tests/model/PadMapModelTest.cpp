// model::PadMapModel: the project's one pad map. docs/SCHEMA.md -> Routing.

#include <catch2/catch_test_macros.hpp>

#include "model/PadMapModel.h"

TEST_CASE ("A PadMap lists its pads in document order", "[model][PadMapModel]")
{
    // Stored out of index order on purpose: child order is never re-sorted.
    const model::PadMapModel padMap (juce::ValueTree::fromXml (
        R"(<PadMap>
             <Pad index="1" inputPitch="37" pitch="42" label="Hat"/>
             <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
           </PadMap>)"));

    const auto pads = padMap.getPads();

    REQUIRE (pads.size() == 2);
    CHECK (pads[0].getIndex() == 1);
    CHECK (pads[0].getLabel() == "Hat");
    CHECK (pads[1].getIndex() == 0);
    CHECK (pads[1].getLabel() == "Kick");
}

TEST_CASE ("An empty PadMap has no pads", "[model][PadMapModel]")
{
    const model::PadMapModel padMap (juce::ValueTree::fromXml ("<PadMap/>"));

    CHECK (padMap.getPads().empty());
}

TEST_CASE ("A PadMap skips children it does not know", "[model][PadMapModel]")
{
    // A file from a newer build may carry children this build has no name
    // for; they are kept in the document and must not turn up as pads.
    const model::PadMapModel padMap (juce::ValueTree::fromXml (
        R"(<PadMap>
             <PadGroup name="Toms"/>
             <Pad index="0" inputPitch="36" pitch="36"/>
           </PadMap>)"));

    const auto pads = padMap.getPads();

    REQUIRE (pads.size() == 1);
    CHECK (pads[0].getInputPitch() == 36);
}
