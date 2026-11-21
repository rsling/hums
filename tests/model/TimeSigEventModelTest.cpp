// model::TimeSigEventModel: a time signature on the Timeline. docs/SCHEMA.md -> Timeline.

#include <catch2/catch_test_macros.hpp>

#include "model/TimeSigEventModel.h"

TEST_CASE ("A TimeSigEvent gives back what is stored", "[model][TimeSigEventModel]")
{
    const model::TimeSigEventModel timeSig (juce::ValueTree::fromXml (
        R"(<TimeSigEvent tick="3840" numerator="7" denominator="8"/>)"));

    CHECK (timeSig.getTick() == 3840);
    CHECK (timeSig.getNumerator() == 7);
    CHECK (timeSig.getDenominator() == 8);
}
